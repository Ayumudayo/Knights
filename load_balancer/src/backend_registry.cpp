#include "load_balancer/backend_registry.hpp"
#include "server/core/util/log.hpp"
#include <algorithm>
#include <functional>

namespace load_balancer {

BackendRegistry::BackendRegistry(std::size_t failure_threshold, std::chrono::seconds retry_cooldown)
    : failure_threshold_(failure_threshold)
    , retry_cooldown_(retry_cooldown) {
}

std::size_t BackendRegistry::set_backends(std::vector<BackendEndpoint> backends) {
    std::unordered_map<std::string, std::size_t> new_index;
    for (std::size_t i = 0; i < backends.size(); ++i) {
        if (backends[i].id.empty()) {
            backends[i].id = "backend-" + std::to_string(i);
        }
        new_index[backends[i].id] = i;
    }

    std::size_t count = backends.size();
    {
        std::lock_guard<std::mutex> lock(hash_mutex_);
        backends_ = std::move(backends);
        backend_index_map_ = std::move(new_index);
    }

    rebuild_hash_ring();

    {
        std::lock_guard<std::mutex> lock(health_mutex_);
        for (auto it = backend_health_.begin(); it != backend_health_.end();) {
            if (backend_index_map_.find(it->first) == backend_index_map_.end()) {
                it = backend_health_.erase(it);
            } else {
                ++it;
            }
        }
    }

    backend_index_.store(0, std::memory_order_relaxed);
    return count;
}

std::vector<BackendEndpoint> BackendRegistry::make_backends_from_records(
    const std::vector<server::state::InstanceRecord>& records, const std::string& self_instance_id) const {
    std::vector<BackendEndpoint> result;
    result.reserve(records.size());
    for (const auto& record : records) {
        if (record.instance_id == self_instance_id) {
            continue;
        }
        if (!record.role.empty()) {
            if (record.role != "server" && record.role != "backend" && record.role != "game_server") {
                continue;
            }
        }
        BackendEndpoint endpoint{};
        endpoint.id = record.instance_id.empty()
            ? (record.host.empty() ? std::string("backend-") + std::to_string(result.size()) : record.host + ":" + std::to_string(record.port))
            : record.instance_id;
        endpoint.host = record.host.empty() ? "127.0.0.1" : record.host;
        endpoint.port = record.port == 0 ? 5000 : record.port;
        result.push_back(std::move(endpoint));
    }
    return result;
}

bool BackendRegistry::apply_snapshot(std::vector<BackendEndpoint> candidates, std::string_view source) {
    if (candidates.empty()) {
        return false;
    }
    if (backends_equal(backends_, candidates)) {
        return false;
    }
    auto count = set_backends(std::move(candidates));
    server::core::log::info("BackendRegistry applied snapshot via " + std::string(source)
        + " count=" + std::to_string(count));
    return true;
}

bool BackendRegistry::is_available(const BackendEndpoint& endpoint, std::chrono::steady_clock::time_point now) {
    std::lock_guard<std::mutex> lock(health_mutex_);
    auto it = backend_health_.find(endpoint.id);
    if (it == backend_health_.end()) {
        return true;
    }
    if (it->second.retry_at != std::chrono::steady_clock::time_point{} && now >= it->second.retry_at) {
        it->second.retry_at = std::chrono::steady_clock::time_point{};
        it->second.failure_count = 0;
        return true;
    }
    if (it->second.retry_at != std::chrono::steady_clock::time_point{} && now < it->second.retry_at) {
        return false;
    }
    return true;
}

void BackendRegistry::mark_success(const std::string& backend_id) {
    if (backend_id.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(health_mutex_);
    auto& health = backend_health_[backend_id];
    health.failure_count = 0;
    health.retry_at = std::chrono::steady_clock::time_point{};
}

void BackendRegistry::mark_failure(const std::string& backend_id) {
    if (backend_id.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(health_mutex_);
    auto& health = backend_health_[backend_id];
    health.failure_count += 1;
    if (health.failure_count >= static_cast<int>(failure_threshold_)) {
        health.retry_at = std::chrono::steady_clock::now() + retry_cooldown_;
    }
}

std::optional<BackendEndpoint> BackendRegistry::select_backend(const std::string& client_id) {
    if (backends_.empty()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();

    if (!client_id.empty() && client_id != "anonymous") {
        std::lock_guard<std::mutex> lock(hash_mutex_);
        if (!hash_ring_.empty()) {
            auto hash = static_cast<std::uint32_t>(std::hash<std::string>{}(client_id));
            auto it = hash_ring_.lower_bound(hash);
            std::size_t inspected = 0;
            if (it == hash_ring_.end()) {
                it = hash_ring_.begin();
            }
            while (inspected < backends_.size() && !hash_ring_.empty()) {
                if (it == hash_ring_.end()) {
                    it = hash_ring_.begin();
                }
                auto idx = it->second;
                if (idx < backends_.size()) {
                    const auto& candidate = backends_[idx];
                    if (is_available(candidate, now)) {
                        return candidate;
                    }
                }
                ++inspected;
                ++it;
            }
        }
    }

    for (std::size_t attempt = 0; attempt < backends_.size(); ++attempt) {
        auto idx = backend_index_.fetch_add(1, std::memory_order_relaxed);
        const auto& candidate = backends_[idx % backends_.size()];
        if (is_available(candidate, now)) {
            return candidate;
        }
    }

    return backends_.front();
}

std::optional<BackendEndpoint> BackendRegistry::find_backend_by_id(const std::string& backend_id) const {
    if (backend_id.empty()) {
        return std::nullopt;
    }
    auto it = backend_index_map_.find(backend_id);
    if (it == backend_index_map_.end()) {
        return std::nullopt;
    }
    if (it->second >= backends_.size()) {
        return std::nullopt;
    }
    return backends_[it->second];
}

bool BackendRegistry::empty() const {
    return backends_.empty();
}

std::size_t BackendRegistry::size() const {
    return backends_.size();
}

const std::vector<BackendEndpoint>& BackendRegistry::get_all() const {
    return backends_;
}

bool BackendRegistry::backends_equal(const std::vector<BackendEndpoint>& lhs, const std::vector<BackendEndpoint>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    std::unordered_map<std::string, std::pair<std::string, std::uint16_t>> map;
    map.reserve(lhs.size());
    for (const auto& ep : lhs) {
        map.emplace(ep.id, std::make_pair(ep.host, ep.port));
    }
    for (const auto& ep : rhs) {
        auto it = map.find(ep.id);
        if (it == map.end()) {
            return false;
        }
        if (it->second.first != ep.host || it->second.second != ep.port) {
            return false;
        }
    }
    return true;
}

void BackendRegistry::rebuild_hash_ring() {
    std::lock_guard<std::mutex> lock(hash_mutex_);
    hash_ring_.clear();
    if (backends_.empty()) {
        return;
    }
    constexpr int kReplicas = 128;
    for (std::size_t i = 0; i < backends_.size(); ++i) {
        const auto& backend = backends_[i];
        for (int replica = 0; replica < kReplicas; ++replica) {
            std::string key = backend.id + "#" + std::to_string(replica);
            auto hash = static_cast<std::uint32_t>(std::hash<std::string>{}(key));
            hash_ring_.emplace(hash, i);
        }
    }
}

} // namespace load_balancer
