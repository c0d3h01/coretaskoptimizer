#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <regex>
#include <sched.h>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace config {
    constexpr const char* LOG_DIR = "/data/adb/modules/task_optimizer/logs/";
    constexpr const char* MAIN_LOG = "/data/adb/modules/task_optimizer/logs/main.log";
    constexpr const char* ERROR_LOG = "/data/adb/modules/task_optimizer/logs/error.log";

    constexpr std::array<std::string_view, 8> HIGH_PRIO_TASKS = {
        "servicemanag", "zygote", "system_server", "surfaceflinger",
        "kblockd", "writeback", "Input", "composer"
    };

    constexpr std::array<std::string_view, 6> RT_TASKS = {
        "kgsl_worker_thread", "crtc_commit", "crtc_event",
        "pp_event", "fts_wq", "nvt_ts_work"
    };

    constexpr std::array<std::string_view, 2> LOW_PRIO_TASKS = {
        "f2fs_gc", "wlan_logging_th"
    };

    constexpr int MAX_RETRIES = 3;
    constexpr int RETRY_DELAY_MS = 50;
}

// Thread-safe logger with rotation
class Logger {
private:
    static inline std::mutex logMutex;
    static constexpr size_t MAX_LOG_SIZE = 1024 * 1024; // 1MB

    static void rotateLog(const char* logFile) {
        try {
            if (fs::file_size(logFile) > MAX_LOG_SIZE) {
                fs::rename(logFile, std::string(logFile) + ".old");
            }
        } catch (...) {}
    }

public:
    static void log(std::string_view message, bool isError = false) noexcept {
        std::lock_guard<std::mutex> lock(logMutex);
        try {
            const char* logFile = isError ? config::ERROR_LOG : config::MAIN_LOG;
            rotateLog(logFile);

            std::ofstream file(logFile, std::ios::app);
            if (file.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                file << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                     << "] " << message << '\n';
            }
        } catch (const std::exception& e) {
            std::cerr << "Logger error: " << e.what() << '\n';
        }
    }
};

// Sanitizer for security
class Sanitizer {
public:
    static std::string sanitizePID(std::string_view input) {
        std::string result;
        for (char c : input) {
            if (std::isdigit(c)) result += c;
        }
        return result;
    }

    static bool isValidPID(pid_t pid) {
        return pid > 0 && pid < 99999 &&
               fs::exists("/proc/" + std::to_string(pid));
    }

    static bool isValidPattern(std::string_view pattern) {
        // Prevent regex DoS and injection
        return pattern.length() < 100 &&
               pattern.find_first_of(";&|`$(){}[]<>") == std::string::npos;
    }
};

// CPU topology detector
class CPUTopology {
private:
    struct CoreInfo {
        std::vector<int> perfCores;
        std::vector<int> effCores;
        int totalCores = 0;
    };

    static CoreInfo detectCores() {
        CoreInfo info;
        try {
            for (int i = 0; i < 16; ++i) {
                std::string freqPath = "/sys/devices/system/cpu/cpu" +
                                      std::to_string(i) + "/cpufreq/cpuinfo_max_freq";
                std::ifstream freqFile(freqPath);
                if (!freqFile.is_open()) break;

                int maxFreq;
                freqFile >> maxFreq;
                info.totalCores++;

                // Cores > 2GHz are performance cores
                if (maxFreq > 2000000) {
                    info.perfCores.push_back(i);
                } else {
                    info.effCores.push_back(i);
                }
            }
        } catch (...) {
            Logger::log("Failed to detect CPU topology, using defaults", true);
            info.perfCores = {4, 5, 6, 7};
            info.effCores = {0, 1, 2, 3};
            info.totalCores = 8;
        }
        return info;
    }

public:
    static cpu_set_t getPerfMask() {
        static CoreInfo info = detectCores();
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for (int core : info.perfCores) {
            CPU_SET(core, &mask);
        }
        return mask;
    }

    static cpu_set_t getEffMask() {
        static CoreInfo info = detectCores();
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for (int core : info.effCores) {
            CPU_SET(core, &mask);
        }
        return mask;
    }

    static cpu_set_t getAllMask() {
        static CoreInfo info = detectCores();
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for (int i = 0; i < info.totalCores; ++i) {
            CPU_SET(i, &mask);
        }
        return mask;
    }
};

// Direct syscall wrapper
class SyscallOptimizer {
private:
    struct OpResult {
        bool success;
        std::string error;
    };

    static OpResult setAffinityDirect(pid_t tid, const cpu_set_t& mask) {
        if (!Sanitizer::isValidPID(tid)) {
            return {false, "Invalid TID"};
        }

        if (sched_setaffinity(tid, sizeof(cpu_set_t), &mask) == 0) {
            return {true, ""};
        }
        return {false, "sched_setaffinity failed: " + std::string(strerror(errno))};
    }

    static OpResult setNiceDirect(pid_t tid, int value) {
        if (!Sanitizer::isValidPID(tid)) {
            return {false, "Invalid TID"};
        }

        errno = 0;
        if (setpriority(PRIO_PROCESS, tid, value) == 0 || errno == 0) {
            return {true, ""};
        }
        return {false, "setpriority failed: " + std::string(strerror(errno))};
    }

    static OpResult setRTDirect(pid_t tid, int priority) {
        if (!Sanitizer::isValidPID(tid)) {
            return {false, "Invalid TID"};
        }

        struct sched_param param;
        param.sched_priority = priority;

        if (sched_setscheduler(tid, SCHED_FIFO, &param) == 0) {
            return {true, ""};
        }
        return {false, "sched_setscheduler failed: " + std::string(strerror(errno))};
    }

    static OpResult setIOPrioDirect(pid_t tid, int ioClass) {
        if (!Sanitizer::isValidPID(tid)) {
            return {false, "Invalid TID"};
        }

        // ioprio_set syscall
        constexpr int IOPRIO_WHO_PROCESS = 1;
        constexpr int IOPRIO_CLASS_SHIFT = 13;
        int ioprio = (ioClass << IOPRIO_CLASS_SHIFT);

        if (syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, tid, ioprio) == 0) {
            return {true, ""};
        }
        return {false, "ioprio_set failed: " + std::string(strerror(errno))};
    }

public:
    static bool setAffinity(pid_t tid, const cpu_set_t& mask) {
        for (int retry = 0; retry < config::MAX_RETRIES; ++retry) {
            auto result = setAffinityDirect(tid, mask);
            if (result.success) return true;

            if (retry < config::MAX_RETRIES - 1) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config::RETRY_DELAY_MS)
                );
            }
        }
        return false;
    }

    static bool setNice(pid_t tid, int value) {
        for (int retry = 0; retry < config::MAX_RETRIES; ++retry) {
            auto result = setNiceDirect(tid, value);
            if (result.success) return true;

            if (retry < config::MAX_RETRIES - 1) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config::RETRY_DELAY_MS)
                );
            }
        }
        return false;
    }

    static bool setRT(pid_t tid, int priority) {
        for (int retry = 0; retry < config::MAX_RETRIES; ++retry) {
            auto result = setRTDirect(tid, priority);
            if (result.success) return true;

            if (retry < config::MAX_RETRIES - 1) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config::RETRY_DELAY_MS)
                );
            }
        }
        return false;
    }

    static bool setIOPrio(pid_t tid, int ioClass) {
        for (int retry = 0; retry < config::MAX_RETRIES; ++retry) {
            auto result = setIOPrioDirect(tid, ioClass);
            if (result.success) return true;

            if (retry < config::MAX_RETRIES - 1) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config::RETRY_DELAY_MS)
                );
            }
        }
        return false;
    }
};

// Process utilities with TOCTOU protection
class ProcessUtils {
public:
    static std::vector<pid_t> getProcessIDs(std::string_view pattern) {
        if (!Sanitizer::isValidPattern(pattern)) {
            Logger::log("Invalid pattern: " + std::string(pattern), true);
            return {};
        }

        std::vector<pid_t> pids;
        std::regex regex(std::string(pattern), std::regex_constants::optimize);

        try {
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator("/proc", ec)) {
                if (ec || !entry.is_directory()) continue;

                const auto filename = entry.path().filename().string();
                if (!std::all_of(filename.begin(), filename.end(), ::isdigit)) continue;

                pid_t pid = std::stoi(filename);
                if (!Sanitizer::isValidPID(pid)) continue;

                std::ifstream commFile(entry.path() / "comm");
                std::string comm;
                if (std::getline(commFile, comm) && std::regex_search(comm, regex)) {
                    pids.push_back(pid);
                }
            }
        } catch (const std::exception& e) {
            Logger::log("Error in getProcessIDs: " + std::string(e.what()), true);
        }

        return pids;
    }

    static std::vector<pid_t> getThreadIDs(pid_t pid) {
        if (!Sanitizer::isValidPID(pid)) return {};

        std::vector<pid_t> tids;
        const std::string taskDir = "/proc/" + std::to_string(pid) + "/task";

        try {
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(taskDir, ec)) {
                if (ec || !entry.is_directory()) continue;

                const auto tidStr = entry.path().filename().string();
                if (std::all_of(tidStr.begin(), tidStr.end(), ::isdigit)) {
                    pid_t tid = std::stoi(tidStr);
                    if (Sanitizer::isValidPID(tid)) {
                        tids.push_back(tid);
                    }
                }
            }
        } catch (const std::exception& e) {
            Logger::log("Error in getThreadIDs: " + std::string(e.what()), true);
        }

        return tids;
    }
};

// Stats tracking
class StatsTracker {
private:
    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};
    std::atomic<int> totalOps{0};

public:
    void recordSuccess() { ++successCount; ++totalOps; }
    void recordFailure() { ++failureCount; ++totalOps; }

    void report() {
        Logger::log("Operations: " + std::to_string(totalOps.load()) +
                   " | Success: " + std::to_string(successCount.load()) +
                   " | Failed: " + std::to_string(failureCount.load()));
    }
};

// Main optimizer
class TaskOptimizer {
private:
    StatsTracker stats;

public:
    void optimizePattern(std::string_view pattern,
                        std::function<bool(pid_t)> optimizer,
                        std::string_view opName) {
        auto pids = ProcessUtils::getProcessIDs(pattern);
        if (pids.empty()) {
            Logger::log("No processes found for: " + std::string(pattern));
            return;
        }

        for (pid_t pid : pids) {
            auto tids = ProcessUtils::getThreadIDs(pid);
            for (pid_t tid : tids) {
                if (optimizer(tid)) {
                    stats.recordSuccess();
                } else {
                    stats.recordFailure();
                    Logger::log("Failed " + std::string(opName) +
                               " for TID " + std::to_string(tid), true);
                }
            }
        }
    }

    void reportStats() {
        stats.report();
    }
};

void optimizeSystem() {
    Logger::log("=== Starting Advanced System Optimization ===");

    TaskOptimizer optimizer;

    // High priority system tasks
    Logger::log("Optimizing high priority tasks...");
    for (const auto& task : config::HIGH_PRIO_TASKS) {
        optimizer.optimizePattern(task, [](pid_t tid) {
            return SyscallOptimizer::setNice(tid, -10) &&
                   SyscallOptimizer::setAffinity(tid, CPUTopology::getPerfMask());
        }, "high_prio");
    }

    // Real-time tasks
    Logger::log("Optimizing real-time tasks...");
    for (const auto& task : config::RT_TASKS) {
        optimizer.optimizePattern(task, [](pid_t tid) {
            return SyscallOptimizer::setRT(tid, 50) &&
                   SyscallOptimizer::setAffinity(tid, CPUTopology::getPerfMask());
        }, "rt");
    }

    // Low priority tasks
    Logger::log("Optimizing low priority tasks...");
    for (const auto& task : config::LOW_PRIO_TASKS) {
        optimizer.optimizePattern(task, [](pid_t tid) {
            return SyscallOptimizer::setNice(tid, 5) &&
                   SyscallOptimizer::setAffinity(tid, CPUTopology::getEffMask()) &&
                   SyscallOptimizer::setIOPrio(tid, 3);
        }, "low_prio");
    }

    optimizer.reportStats();
    Logger::log("=== System Optimization Completed ===");
}

int main() {
    try {
        std::error_code ec;
        fs::create_directories(config::LOG_DIR, ec);
        if (ec) {
            std::cerr << "Failed to create log directory: " << ec.message() << '\n';
            return 1;
        }

        optimizeSystem();
        return 0;

    } catch (const std::exception& e) {
        Logger::log("Critical error: " + std::string(e.what()), true);
        return 1;
    } catch (...) {
        Logger::log("Unknown critical error", true);
        return 1;
    }
}
