#pragma once

#include <any>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

namespace server::core::util::services {

namespace detail {

std::string normalize_type_name(const std::type_info& info);

template <typename T>
std::string type_key() {
    static_assert(!std::is_void_v<T>, "void 타입은 서비스 레지스트리에 등록할 수 없습니다.");
    return normalize_type_name(typeid(T));
}

} // namespace detail

class Registry {
public:
    static Registry& instance();

    template <typename T>
    void set(std::shared_ptr<T> service) {
        static_assert(!std::is_void_v<T>, "void 타입은 서비스 레지스트리에 등록할 수 없습니다.");
        if (!service) {
            throw std::invalid_argument("서비스 포인터가 null 입니다.");
        }
        set_any(detail::type_key<T>(), std::move(service));
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> emplace(Args&&... args) {
        auto service = std::make_shared<T>(std::forward<Args>(args)...);
        set<T>(service);
        return service;
    }

    template <typename T>
    std::shared_ptr<T> get() const {
        auto value = get_any(detail::type_key<T>());
        if (!value.has_value()) {
            return nullptr;
        }
        try {
            return std::any_cast<std::shared_ptr<T>>(value);
        } catch (const std::bad_any_cast&) {
            throw std::runtime_error("요청한 타입과 등록된 서비스 타입이 일치하지 않습니다: " + detail::type_key<T>());
        }
    }

    template <typename T>
    T& require() const {
        auto ptr = get<T>();
        if (!ptr) {
            throw std::runtime_error("요청한 서비스가 등록되지 않았습니다: " + detail::type_key<T>());
        }
        return *ptr;
    }

    template <typename T>
    bool has() const {
        return has_impl(detail::type_key<T>());
    }

    void clear();

private:
    Registry() = default;

    void set_any(std::string key, std::any value);
    std::any get_any(const std::string& key) const;
    bool has_impl(const std::string& key) const;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> services_;
};

template <typename T>
void set(std::shared_ptr<T> service) {
    Registry::instance().set<T>(std::move(service));
}

template <typename T, typename... Args>
std::shared_ptr<T> emplace(Args&&... args) {
    return Registry::instance().emplace<T>(std::forward<Args>(args)...);
}

template <typename T>
std::shared_ptr<T> get() {
    return Registry::instance().get<T>();
}

template <typename T>
T& require() {
    return Registry::instance().require<T>();
}

template <typename T>
bool has() {
    return Registry::instance().has<T>();
}

inline void clear() {
    Registry::instance().clear();
}

} // namespace server::core::util::services

