#pragma once
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

// Format a duration in milliseconds as a human-readable string
inline std::string format_duration(long long ms) {
    if (ms < 1000) {
        return std::to_string(ms) + " ms";
    }
    double seconds = ms / 1000.0;
    if (seconds < 60) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << seconds << " s";
        return oss.str();
    }
    int total_seconds = static_cast<int>(seconds);
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int secs = total_seconds % 60;
    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << "h " << minutes << "m " << secs << "s";
    } else {
        oss << minutes << "m " << secs << "s";
    }
    return oss.str();
}
