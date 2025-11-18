#include "core/profiling.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdlib>  // For getenv

namespace keystone {
namespace core {

// Static helper to get global section data
std::map<std::string, ProfilingSession::SectionData>& ProfilingSession::getSectionData() {
    static std::map<std::string, SectionData> data;
    return data;
}

std::mutex& ProfilingSession::getGlobalMutex() {
    static std::mutex mutex;
    return mutex;
}

bool ProfilingSession::checkEnabled() {
    static bool enabled = []() {
        const char* env = std::getenv("KEYSTONE_PROFILE");
        return env != nullptr && std::string(env) == "1";
    }();
    return enabled;
}

bool ProfilingSession::isEnabled() {
    return checkEnabled();
}

ProfilingSession::ProfilingSession(const std::string& section_name, bool enabled)
    : section_name_(section_name),
      start_time_(std::chrono::steady_clock::now()),
      enabled_(enabled),
      ended_(false) {
}

ProfilingSession ProfilingSession::start(const std::string& section_name) {
    return ProfilingSession(section_name, checkEnabled());
}

void ProfilingSession::end() {
    if (!enabled_ || ended_) {
        return;  // Already ended or profiling disabled
    }

    ended_ = true;

    auto end_time = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration<double, std::micro>(
        end_time - start_time_
    ).count();

    recordDuration(section_name_, duration_us);
}

ProfilingSession::~ProfilingSession() {
    end();  // Safe to call multiple times
}

ProfilingSession::ProfilingSession(ProfilingSession&& other) noexcept
    : section_name_(std::move(other.section_name_)),
      start_time_(other.start_time_),
      enabled_(other.enabled_),
      ended_(other.ended_) {
    other.ended_ = true;  // Prevent double-end
}

ProfilingSession& ProfilingSession::operator=(ProfilingSession&& other) noexcept {
    if (this != &other) {
        end();  // End current session
        section_name_ = std::move(other.section_name_);
        start_time_ = other.start_time_;
        enabled_ = other.enabled_;
        ended_ = other.ended_;
        other.ended_ = true;
    }
    return *this;
}

void ProfilingSession::recordDuration(const std::string& section_name, double duration_us) {
    std::lock_guard<std::mutex> global_lock(getGlobalMutex());

    auto& data = getSectionData();
    auto& section = data[section_name];

    std::lock_guard<std::mutex> section_lock(section.mutex);
    section.durations_us.push_back(duration_us);
}

std::optional<ProfilingSession::SectionStats> ProfilingSession::getStats(
    const std::string& section_name) {

    std::lock_guard<std::mutex> global_lock(getGlobalMutex());

    auto& data = getSectionData();
    auto it = data.find(section_name);
    if (it == data.end()) {
        return std::nullopt;
    }

    auto& section = it->second;
    std::lock_guard<std::mutex> section_lock(section.mutex);

    if (section.durations_us.empty()) {
        return std::nullopt;
    }

    // Calculate statistics
    auto durations = section.durations_us;  // Copy for sorting
    std::sort(durations.begin(), durations.end());

    SectionStats stats;
    stats.sample_count = durations.size();
    stats.min_us = durations.front();
    stats.max_us = durations.back();

    // Mean
    double sum = 0.0;
    for (double d : durations) {
        sum += d;
    }
    stats.mean_us = sum / durations.size();

    // Percentiles
    auto percentile = [&](double p) -> double {
        size_t index = static_cast<size_t>(p * (durations.size() - 1));
        return durations[index];
    };

    stats.p50_us = percentile(0.50);
    stats.p95_us = percentile(0.95);
    stats.p99_us = percentile(0.99);

    return stats;
}

std::string ProfilingSession::generateReport() {
    std::lock_guard<std::mutex> global_lock(getGlobalMutex());

    auto& data = getSectionData();

    if (data.empty()) {
        return "No profiling data collected.\n"
               "Set KEYSTONE_PROFILE=1 environment variable to enable profiling.\n";
    }

    std::ostringstream oss;
    oss << "\n=== Performance Profiling Report ===\n\n";
    oss << std::left << std::setw(30) << "Section"
        << std::right << std::setw(10) << "Samples"
        << std::setw(12) << "Min (µs)"
        << std::setw(12) << "Mean (µs)"
        << std::setw(12) << "P50 (µs)"
        << std::setw(12) << "P95 (µs)"
        << std::setw(12) << "P99 (µs)"
        << std::setw(12) << "Max (µs)"
        << "\n";
    oss << std::string(112, '-') << "\n";

    for (const auto& [section_name, section_data] : data) {
        auto stats_opt = getStats(section_name);
        if (!stats_opt) continue;

        const auto& stats = *stats_opt;

        oss << std::left << std::setw(30) << section_name
            << std::right << std::setw(10) << stats.sample_count
            << std::setw(12) << std::fixed << std::setprecision(2) << stats.min_us
            << std::setw(12) << std::fixed << std::setprecision(2) << stats.mean_us
            << std::setw(12) << std::fixed << std::setprecision(2) << stats.p50_us
            << std::setw(12) << std::fixed << std::setprecision(2) << stats.p95_us
            << std::setw(12) << std::fixed << std::setprecision(2) << stats.p99_us
            << std::setw(12) << std::fixed << std::setprecision(2) << stats.max_us
            << "\n";
    }

    oss << std::string(112, '-') << "\n";
    oss << "\nAll durations in microseconds (µs)\n";

    return oss.str();
}

void ProfilingSession::reset() {
    std::lock_guard<std::mutex> global_lock(getGlobalMutex());
    getSectionData().clear();
}

} // namespace core
} // namespace keystone
