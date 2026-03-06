#include "session_client.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace loadgen {
namespace detail {

enum class SessionMode {
    kLoginOnly,
    kChat,
    kPing,
};

struct GroupConfig {
    std::string name;
    SessionMode mode{SessionMode::kLoginOnly};
    std::uint32_t count{0};
    double rate_per_sec{0.0};
    bool join_room{false};
};

struct ScenarioConfig {
    std::string name;
    std::uint32_t sessions{0};
    std::uint32_t ramp_up_ms{0};
    std::uint32_t duration_ms{0};
    std::string room{"lobby"};
    std::string room_password;
    bool unique_room_per_run{true};
    std::uint32_t message_bytes{64};
    std::string login_prefix{"loadgen"};
    std::uint32_t connect_timeout_ms{5000};
    std::uint32_t read_timeout_ms{5000};
    std::vector<GroupConfig> groups;
};

struct CliOptions {
    std::string host;
    std::uint16_t port{0};
    fs::path scenario_path;
    fs::path report_path;
    std::uint32_t seed{0};
    bool verbose{false};
};

struct RunSummary {
    std::string scenario;
    std::string host;
    std::string room;
    std::uint16_t port{0};
    std::uint32_t seed{0};
    std::uint32_t sessions{0};
    std::uint32_t connected_sessions{0};
    std::uint32_t authenticated_sessions{0};
    std::uint32_t joined_sessions{0};
    std::uint64_t elapsed_ms{0};
    std::uint64_t success_count{0};
    std::uint64_t error_count{0};
    std::uint64_t connect_failures{0};
    std::uint64_t login_failures{0};
    std::uint64_t join_failures{0};
    std::uint64_t chat_success{0};
    std::uint64_t chat_errors{0};
    std::uint64_t ping_success{0};
    std::uint64_t ping_errors{0};
    double throughput_rps{0.0};
    double latency_p50_ms{0.0};
    double latency_p95_ms{0.0};
    double latency_p99_ms{0.0};
    double latency_max_ms{0.0};
    TransportStats transport;
};

struct GroupAssignment {
    std::string name;
    SessionMode mode{SessionMode::kLoginOnly};
    double rate_per_sec{0.0};
    bool join_room{false};
};

class MetricsCollector {
public:
    void record_connected() {
        std::lock_guard<std::mutex> lock(mu_);
        ++connected_sessions_;
    }

    void record_authenticated() {
        std::lock_guard<std::mutex> lock(mu_);
        ++authenticated_sessions_;
    }

    void record_joined() {
        std::lock_guard<std::mutex> lock(mu_);
        ++joined_sessions_;
    }

    void record_connect_failure() {
        std::lock_guard<std::mutex> lock(mu_);
        ++error_count_;
        ++connect_failures_;
    }

    void record_login_failure() {
        std::lock_guard<std::mutex> lock(mu_);
        ++error_count_;
        ++login_failures_;
    }

    void record_join_failure() {
        std::lock_guard<std::mutex> lock(mu_);
        ++error_count_;
        ++join_failures_;
    }

    void record_chat_success(double latency_ms) {
        std::lock_guard<std::mutex> lock(mu_);
        ++success_count_;
        ++chat_success_;
        latencies_ms_.push_back(latency_ms);
    }

    void record_chat_failure() {
        std::lock_guard<std::mutex> lock(mu_);
        ++error_count_;
        ++chat_errors_;
    }

    void record_ping_success(double latency_ms) {
        std::lock_guard<std::mutex> lock(mu_);
        ++success_count_;
        ++ping_success_;
        latencies_ms_.push_back(latency_ms);
    }

    void record_ping_failure() {
        std::lock_guard<std::mutex> lock(mu_);
        ++error_count_;
        ++ping_errors_;
    }

    void record_runtime_failure() {
        std::lock_guard<std::mutex> lock(mu_);
        ++error_count_;
    }

    RunSummary finalize(const ScenarioConfig& scenario,
                        const CliOptions& cli,
                        const std::string& resolved_room,
                        std::uint64_t elapsed_ms,
                        const std::vector<std::unique_ptr<SessionClient>>& clients) const {
        RunSummary summary;
        summary.scenario = scenario.name;
        summary.host = cli.host;
        summary.room = resolved_room;
        summary.port = cli.port;
        summary.seed = cli.seed;
        summary.sessions = scenario.sessions;
        summary.elapsed_ms = elapsed_ms;

        std::vector<double> latencies;
        {
            std::lock_guard<std::mutex> lock(mu_);
            summary.connected_sessions = connected_sessions_;
            summary.authenticated_sessions = authenticated_sessions_;
            summary.joined_sessions = joined_sessions_;
            summary.success_count = success_count_;
            summary.error_count = error_count_;
            summary.connect_failures = connect_failures_;
            summary.login_failures = login_failures_;
            summary.join_failures = join_failures_;
            summary.chat_success = chat_success_;
            summary.chat_errors = chat_errors_;
            summary.ping_success = ping_success_;
            summary.ping_errors = ping_errors_;
            latencies = latencies_ms_;
        }

        for (const auto& client : clients) {
            if (client == nullptr) {
                continue;
            }
            const auto transport = client->transport_stats();
            summary.transport.connect_failures += transport.connect_failures;
            summary.transport.read_timeouts += transport.read_timeouts;
            summary.transport.disconnects += transport.disconnects;
        }

        const auto elapsed_seconds = std::max(0.001, static_cast<double>(elapsed_ms) / 1000.0);
        summary.throughput_rps = static_cast<double>(summary.success_count) / elapsed_seconds;

        if (!latencies.empty()) {
            std::sort(latencies.begin(), latencies.end());
            summary.latency_p50_ms = percentile(latencies, 50.0);
            summary.latency_p95_ms = percentile(latencies, 95.0);
            summary.latency_p99_ms = percentile(latencies, 99.0);
            summary.latency_max_ms = latencies.back();
        }
        return summary;
    }

private:
    static double percentile(const std::vector<double>& sorted_values, double p) {
        if (sorted_values.empty()) {
            return 0.0;
        }
        const auto rank = static_cast<std::size_t>(
            std::llround((p / 100.0) * static_cast<double>(sorted_values.size() - 1)));
        return sorted_values[rank];
    }

    mutable std::mutex mu_;
    std::uint32_t connected_sessions_{0};
    std::uint32_t authenticated_sessions_{0};
    std::uint32_t joined_sessions_{0};
    std::uint64_t success_count_{0};
    std::uint64_t error_count_{0};
    std::uint64_t connect_failures_{0};
    std::uint64_t login_failures_{0};
    std::uint64_t join_failures_{0};
    std::uint64_t chat_success_{0};
    std::uint64_t chat_errors_{0};
    std::uint64_t ping_success_{0};
    std::uint64_t ping_errors_{0};
    std::vector<double> latencies_ms_;
};

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

SessionMode parse_mode(const std::string& raw) {
    const auto mode = to_lower(raw);
    if (mode == "chat") {
        return SessionMode::kChat;
    }
    if (mode == "ping") {
        return SessionMode::kPing;
    }
    if (mode == "login_only" || mode == "idle") {
        return SessionMode::kLoginOnly;
    }
    throw std::runtime_error("unsupported session mode: " + raw);
}

std::string mode_name(SessionMode mode) {
    switch (mode) {
    case SessionMode::kChat:
        return "chat";
    case SessionMode::kPing:
        return "ping";
    case SessionMode::kLoginOnly:
        return "login_only";
    }
    return "unknown";
}

std::string make_chat_message(const ScenarioConfig& scenario,
                              std::uint32_t session_index,
                              std::uint64_t iteration) {
    std::ostringstream stream;
    stream << scenario.name << "|session=" << session_index << "|iteration=" << iteration << "|";
    std::string message = stream.str();
    if (message.size() < scenario.message_bytes) {
        message.append(scenario.message_bytes - message.size(), 'x');
    } else if (message.size() > scenario.message_bytes) {
        message.resize(scenario.message_bytes);
    }
    return message;
}

void print_usage() {
    std::cerr
        << "Usage: tcp_loadgen --host <host> --port <port> --scenario <path> --report <path> "
        << "[--seed <u32>] [--verbose]\n";
}

CliOptions parse_cli(int argc, char** argv) {
    CliOptions options;
    options.seed = static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFFu);

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto require_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--host") {
            options.host = require_value("--host");
            continue;
        }
        if (arg == "--port") {
            const auto parsed = std::stoul(require_value("--port"));
            if (parsed > 65535) {
                throw std::runtime_error("port out of range");
            }
            options.port = static_cast<std::uint16_t>(parsed);
            continue;
        }
        if (arg == "--scenario") {
            options.scenario_path = require_value("--scenario");
            continue;
        }
        if (arg == "--report") {
            options.report_path = require_value("--report");
            continue;
        }
        if (arg == "--seed") {
            options.seed = static_cast<std::uint32_t>(std::stoul(require_value("--seed")));
            continue;
        }
        if (arg == "--verbose") {
            options.verbose = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        }
        throw std::runtime_error("unknown argument: " + std::string(arg));
    }

    if (options.host.empty() || options.port == 0 || options.scenario_path.empty() || options.report_path.empty()) {
        throw std::runtime_error("missing required arguments");
    }
    return options;
}

ScenarioConfig load_scenario(const fs::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open scenario: " + path.string());
    }

    json document;
    input >> document;

    ScenarioConfig scenario;
    scenario.name = document.value("name", path.stem().string());
    scenario.sessions = document.at("sessions").get<std::uint32_t>();
    scenario.ramp_up_ms = document.value("ramp_up_ms", 0u);
    scenario.duration_ms = document.at("duration_ms").get<std::uint32_t>();
    scenario.room = document.value("room", std::string("lobby"));
    scenario.room_password = document.value("room_password", std::string());
    scenario.unique_room_per_run = document.value("unique_room_per_run", true);
    scenario.message_bytes = document.value("message_bytes", 64u);
    scenario.login_prefix = document.value("login_prefix", std::string("loadgen"));
    scenario.connect_timeout_ms = document.value("connect_timeout_ms", 5000u);
    scenario.read_timeout_ms = document.value("read_timeout_ms", 5000u);

    if (document.contains("groups")) {
        for (const auto& group_json : document.at("groups")) {
            GroupConfig group;
            group.name = group_json.value("name", std::string());
            group.mode = parse_mode(group_json.at("mode").get<std::string>());
            group.count = group_json.at("count").get<std::uint32_t>();
            group.rate_per_sec = group_json.value("rate_per_sec", 0.0);
            group.join_room = group_json.value("join_room", group.mode == SessionMode::kChat);
            scenario.groups.push_back(std::move(group));
        }
    } else {
        GroupConfig group;
        group.name = "default";
        group.mode = SessionMode::kChat;
        group.count = scenario.sessions;
        group.rate_per_sec = document.value("message_rate_per_sec", 1.0);
        group.join_room = true;
        scenario.groups.push_back(std::move(group));
    }

    if (scenario.sessions == 0) {
        throw std::runtime_error("scenario sessions must be > 0");
    }
    if (scenario.duration_ms == 0) {
        throw std::runtime_error("scenario duration_ms must be > 0");
    }
    if (scenario.message_bytes == 0) {
        throw std::runtime_error("scenario message_bytes must be > 0");
    }
    if (scenario.groups.empty()) {
        throw std::runtime_error("scenario groups must not be empty");
    }

    std::uint32_t total_group_sessions = 0;
    for (const auto& group : scenario.groups) {
        total_group_sessions += group.count;
        if ((group.mode == SessionMode::kChat || group.mode == SessionMode::kPing) && group.rate_per_sec <= 0.0) {
            throw std::runtime_error("chat/ping groups require rate_per_sec > 0");
        }
    }
    if (total_group_sessions != scenario.sessions) {
        throw std::runtime_error("sum(groups.count) must equal sessions");
    }
    return scenario;
}

std::vector<GroupAssignment> expand_assignments(const ScenarioConfig& scenario) {
    std::vector<GroupAssignment> assignments;
    assignments.reserve(scenario.sessions);
    for (const auto& group : scenario.groups) {
        for (std::uint32_t i = 0; i < group.count; ++i) {
            assignments.push_back(GroupAssignment{
                .name = group.name,
                .mode = group.mode,
                .rate_per_sec = group.rate_per_sec,
                .join_room = group.join_room,
            });
        }
    }
    return assignments;
}

void maybe_log(bool verbose, const std::string& message) {
    if (verbose) {
        std::cout << message << '\n';
    }
}

void run_session_workload(SessionClient& client,
                          const ScenarioConfig& scenario,
                          const std::string& run_room,
                          const GroupAssignment& assignment,
                          std::uint32_t session_index,
                          std::uint32_t seed,
                          const std::chrono::steady_clock::time_point deadline,
                          MetricsCollector& metrics,
                          bool verbose) {
    if (assignment.mode == SessionMode::kLoginOnly) {
        while (std::chrono::steady_clock::now() < deadline) {
            if (!client.is_connected()) {
                metrics.record_runtime_failure();
                maybe_log(verbose, "session " + std::to_string(session_index) + " disconnected during login_only");
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        return;
    }

    const auto interval = std::chrono::duration<double>(1.0 / assignment.rate_per_sec);
    const auto interval_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
    std::mt19937 rng(seed ^ (session_index * 2654435761u));
    const auto initial_jitter_ms = std::uniform_int_distribution<int>(
        0,
        std::max(0, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(interval).count())))
                                       (rng);
    auto next_action = std::chrono::steady_clock::now() + std::chrono::milliseconds(initial_jitter_ms);
    std::uint64_t iteration = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto now = std::chrono::steady_clock::now();
        if (next_action > now) {
            std::this_thread::sleep_until(std::min(next_action, deadline));
            continue;
        }

        if (assignment.mode == SessionMode::kChat) {
            const auto message = make_chat_message(scenario, session_index, iteration++);
            const auto started = std::chrono::steady_clock::now();
            if (!client.send_chat_and_wait_echo(run_room, message)) {
                metrics.record_chat_failure();
                maybe_log(verbose,
                          "session " + std::to_string(session_index) + " chat failed: " + client.last_error());
                return;
            }
            const auto elapsed_ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            metrics.record_chat_success(elapsed_ms);
        } else if (assignment.mode == SessionMode::kPing) {
            const auto started = std::chrono::steady_clock::now();
            if (!client.send_ping_and_wait_pong()) {
                metrics.record_ping_failure();
                maybe_log(verbose,
                          "session " + std::to_string(session_index) + " ping failed: " + client.last_error());
                return;
            }
            const auto elapsed_ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            metrics.record_ping_success(elapsed_ms);
        }

        next_action = std::chrono::steady_clock::now() + interval_duration;
    }
}

json to_json(const RunSummary& summary) {
    return json{
        {"scenario", summary.scenario},
        {"host", summary.host},
        {"room", summary.room},
        {"port", summary.port},
        {"seed", summary.seed},
        {"sessions", summary.sessions},
        {"connected_sessions", summary.connected_sessions},
        {"authenticated_sessions", summary.authenticated_sessions},
        {"joined_sessions", summary.joined_sessions},
        {"elapsed_ms", summary.elapsed_ms},
        {"success_count", summary.success_count},
        {"error_count", summary.error_count},
        {"setup",
         {
             {"connect_failures", summary.connect_failures},
             {"login_failures", summary.login_failures},
             {"join_failures", summary.join_failures},
         }},
        {"operations",
         {
             {"chat_success", summary.chat_success},
             {"chat_errors", summary.chat_errors},
             {"ping_success", summary.ping_success},
             {"ping_errors", summary.ping_errors},
         }},
        {"throughput_rps", summary.throughput_rps},
        {"latency_ms",
         {
             {"p50", summary.latency_p50_ms},
             {"p95", summary.latency_p95_ms},
             {"p99", summary.latency_p99_ms},
             {"max", summary.latency_max_ms},
         }},
        {"transport",
         {
             {"connect_failures", summary.transport.connect_failures},
             {"read_timeouts", summary.transport.read_timeouts},
             {"disconnects", summary.transport.disconnects},
         }},
    };
}

void write_report(const fs::path& report_path, const json& report) {
    if (!report_path.parent_path().empty()) {
        fs::create_directories(report_path.parent_path());
    }

    std::ofstream output(report_path);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open report path: " + report_path.string());
    }
    output << std::setw(2) << report << '\n';
}

RunSummary run_scenario(const ScenarioConfig& scenario, const CliOptions& cli) {
    MetricsCollector metrics;
    auto assignments = expand_assignments(scenario);
    std::vector<std::unique_ptr<SessionClient>> clients;
    clients.reserve(assignments.size());
    std::vector<std::thread> workers;
    workers.reserve(assignments.size());
    const auto resolved_room =
        (!scenario.unique_room_per_run || scenario.room == "lobby")
            ? scenario.room
            : scenario.room + "_" + std::to_string(cli.seed);

    const auto run_started = std::chrono::steady_clock::now();
    const auto run_deadline =
        run_started + std::chrono::milliseconds(scenario.ramp_up_ms + scenario.duration_ms);
    const auto ramp_delay = scenario.sessions > 0
                                ? std::chrono::milliseconds(scenario.ramp_up_ms / scenario.sessions)
                                : std::chrono::milliseconds(0);

    for (std::uint32_t session_index = 0; session_index < assignments.size(); ++session_index) {
        const auto& assignment = assignments[session_index];
        auto client = std::make_unique<SessionClient>(ClientOptions{
            .connect_timeout_ms = scenario.connect_timeout_ms,
            .read_timeout_ms = scenario.read_timeout_ms,
        });

        maybe_log(cli.verbose,
                  "setup session=" + std::to_string(session_index) + " mode=" + mode_name(assignment.mode));

        if (!client->connect(cli.host, cli.port)) {
            metrics.record_connect_failure();
            maybe_log(cli.verbose,
                      "connect failed for session " + std::to_string(session_index) + ": " + client->last_error());
            clients.push_back(std::move(client));
            if (ramp_delay.count() > 0) {
                std::this_thread::sleep_for(ramp_delay);
            }
            continue;
        }
        metrics.record_connected();

        LoginResult login_result;
        const auto user = scenario.login_prefix + "_" + std::to_string(session_index);
        if (!client->login(user, {}, &login_result)) {
            metrics.record_login_failure();
            maybe_log(cli.verbose,
                      "login failed for session " + std::to_string(session_index) + ": " + client->last_error());
            client->close();
            clients.push_back(std::move(client));
            if (ramp_delay.count() > 0) {
                std::this_thread::sleep_for(ramp_delay);
            }
            continue;
        }
        metrics.record_authenticated();

        if (assignment.join_room) {
            SnapshotResult snapshot;
            if (!client->join(resolved_room, scenario.room_password, &snapshot)) {
                metrics.record_join_failure();
                maybe_log(cli.verbose,
                          "join failed for session " + std::to_string(session_index) + ": " + client->last_error());
                client->close();
                clients.push_back(std::move(client));
                if (ramp_delay.count() > 0) {
                    std::this_thread::sleep_for(ramp_delay);
                }
                continue;
            }
            metrics.record_joined();
        }

        auto* client_ptr = client.get();
        workers.emplace_back([client_ptr,
                              &scenario,
                              &resolved_room,
                              assignment,
                              session_index,
                              seed = cli.seed,
                              run_deadline,
                              &metrics,
                              verbose = cli.verbose]() {
            run_session_workload(
                *client_ptr, scenario, resolved_room, assignment, session_index, seed, run_deadline, metrics, verbose);
        });
        clients.push_back(std::move(client));

        if (ramp_delay.count() > 0) {
            std::this_thread::sleep_for(ramp_delay);
        }
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    for (auto& client : clients) {
        if (client != nullptr) {
            client->close();
        }
    }

    const auto elapsed_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - run_started)
            .count());
    return metrics.finalize(scenario, cli, resolved_room, elapsed_ms, clients);
}

}  // namespace detail
}  // namespace loadgen

int main(int argc, char** argv) {
    try {
        const auto cli = loadgen::detail::parse_cli(argc, argv);
        const auto scenario = loadgen::detail::load_scenario(cli.scenario_path);
        const auto summary = loadgen::detail::run_scenario(scenario, cli);
        const auto report = loadgen::detail::to_json(summary);
        loadgen::detail::write_report(cli.report_path, report);

        std::cout << "tcp_loadgen_summary"
                  << " scenario=" << summary.scenario
                  << " sessions=" << summary.sessions
                  << " connected=" << summary.connected_sessions
                  << " authenticated=" << summary.authenticated_sessions
                  << " joined=" << summary.joined_sessions
                  << " success=" << summary.success_count
                  << " errors=" << summary.error_count
                  << " throughput_rps=" << std::fixed << std::setprecision(2) << summary.throughput_rps
                  << " p95_ms=" << summary.latency_p95_ms
                  << " report=" << cli.report_path.string()
                  << '\n';

        return summary.error_count == 0 && summary.connected_sessions > 0 ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "tcp_loadgen error: " << ex.what() << '\n';
        return 1;
    }
}
