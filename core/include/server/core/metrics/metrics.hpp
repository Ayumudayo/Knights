#pragma once

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

namespace server::core::metrics {

using Label = std::pair<std::string, std::string>;
using Labels = std::initializer_list<Label>;

class Counter {
public:
    virtual ~Counter() = default;
    virtual void inc(double value = 1.0, Labels labels = {}) = 0;
};

class Gauge {
public:
    virtual ~Gauge() = default;
    virtual void set(double value, Labels labels = {}) = 0;
    virtual void inc(double value = 1.0, Labels labels = {}) = 0;
    virtual void dec(double value = 1.0, Labels labels = {}) = 0;
};

class Histogram {
public:
    virtual ~Histogram() = default;
    virtual void observe(double value, Labels labels = {}) = 0;
};

// 등록된 외부 구현이 없으면 no-op 객체를 돌려준다.
Counter& get_counter(const std::string& name);
Gauge& get_gauge(const std::string& name);
Histogram& get_histogram(const std::string& name);

} // namespace server::core::metrics

