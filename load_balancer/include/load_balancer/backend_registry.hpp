#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <map>
#include <atomic>
#include <optional>
#include "server/state/instance_registry.hpp"

namespace load_balancer {

struct BackendEndpoint {
    std::string id;
    std::string host;
    std::uint16_t port{0};
};

struct BackendHealth {
    int failure_count{0};
    std::chrono::steady_clock::time_point retry_at{};
};

class BackendRegistry {
public:
    BackendRegistry(std::size_t failure_threshold, std::chrono::seconds retry_cooldown);

    std::size_t set_backends(std::vector<BackendEndpoint> backends);
    std::vector<BackendEndpoint> make_backends_from_records(const std::vector<server::state::InstanceRecord>& records, const std::string& self_instance_id) const;
    
    bool apply_snapshot(std::vector<BackendEndpoint> candidates, std::string_view source);
    bool is_available(const BackendEndpoint& endpoint, std::chrono::steady_clock::time_point now);
    
    void mark_success(const std::string& backend_id);
    void mark_failure(const std::string& backend_id);
    
    std::optional<BackendEndpoint> select_backend(const std::string& client_id);
    std::optional<BackendEndpoint> find_backend_by_id(const std::string& backend_id) const;
    
    bool empty() const;
    std::size_t size() const;
    const std::vector<BackendEndpoint>& get_all() const;

    static bool backends_equal(const std::vector<BackendEndpoint>& lhs, const std::vector<BackendEndpoint>& rhs);

private:
    void rebuild_hash_ring();

    std::vector<BackendEndpoint> backends_;
    std::unordered_map<std::string, std::size_t> backend_index_map_;
    std::map<std::uint32_t, std::size_t> hash_ring_;
    mutable std::mutex hash_mutex_;

    std::unordered_map<std::string, BackendHealth> backend_health_;
    mutable std::mutex health_mutex_;

    std::atomic<std::size_t> backend_index_{0};

    std::size_t failure_threshold_;
    std::chrono::seconds retry_cooldown_;

public:
    void set_failure_threshold(std::size_t threshold) { failure_threshold_ = threshold; }
    void set_retry_cooldown(std::chrono::seconds cooldown) { retry_cooldown_ = cooldown; }
};

} // namespace load_balancer
