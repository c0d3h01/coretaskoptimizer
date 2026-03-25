# Core Task Optimizer

[![Android Build Test](https://github.com/c0d3h01/CoreTaskOptimizer/actions/workflows/checks.yml/badge.svg?branch=main)](https://github.com/c0d3h01/CoreTaskOptimizer/actions/workflows/checks.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Magisk](https://img.shields.io/badge/Magisk-v20.4%2B-green)](https://github.com/topjohnwu/Magisk)
[![Android](https://img.shields.io/badge/Android-8.0%2B-brightgreen)](https://www.android.com)

A native C++ Android root module for **Magisk**, **KernelSU**, and **APatch** that improves system responsiveness by applying precise CPU affinity, scheduler, and I/O priority policies to critical system tasks via direct Linux syscalls — with zero persistent overhead after the initial boot run.

---

## Table of Contents

- [Why This Module](#why-this-module)
- [How It Works](#how-it-works)
- [Optimization Targets](#optimization-targets)
- [Requirements](#requirements)
- [Installation](#installation)
- [Verifying Operation](#verifying-operation)
- [Build from Source](#build-from-source)
- [Safety Notice](#safety-notice)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

---

## Why This Module

Most Android performance tweaks rely on shell scripts calling `renice` or writing to `/proc` sysfs entries. This introduces process spawn overhead, depends on userspace tooling availability, and applies policies at the process level rather than the thread level.

Core Task Optimizer takes a different approach:

- **Native C++ binary** — compiled per ABI, no shell interpreter overhead
- **Direct syscalls** — `sched_setaffinity`, `sched_setscheduler`, `setpriority`, `ioprio_set` called directly
- **Thread-level precision** — policies applied to every thread in `/proc/<pid>/task`, not just the parent process
- **One-shot at boot** — runs once after boot completion, then exits. No daemons, no polling loops, no persistent memory footprint
- **Dynamic CPU topology detection** — reads `/sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq` at runtime to correctly identify performance and efficiency cores on any SoC

---

## How It Works

```
Boot completed + 30s grace period
        │
        ▼
task_optimizer binary starts
        │
        ├─► Detect CPU topology (perf vs efficiency cores)
        │
        ├─► Scan /proc for matching process names
        │
        ├─► For each match: enumerate /proc/<pid>/task threads
        │
        ├─► Apply per-group policy to each thread ID
        │   ├── HIGH_PRIO  → nice -10, affinity → perf cores
        │   ├── RT_TASKS   → SCHED_FIFO prio 50, affinity → perf cores
        │   └── LOW_PRIO   → nice +5, affinity → eff cores, ioprio class 3
        │
        └─► Log results to /data/adb/modules/task_optimizer/logs/
```

The binary matches process names against entries in `/proc/<pid>/comm` using `std::regex`. All operations include retry logic (up to 3 attempts) to handle transient failures during the early boot window.

---

## Optimization Targets

| Group | Processes | Policy Applied |
|---|---|---|
| High Priority | `servicemanager`, `zygote`, `system_server`, `surfaceflinger`, `kblockd`, `writeback`, `composer` | nice -10, perf core affinity |
| Real-Time | `kgsl_worker_thread`, `crtc_commit`, `crtc_event`, `pp_event`, `fts_wq`, `nvt_ts_work` | SCHED_FIFO priority 50, perf core affinity |
| Background | `f2fs_gc`, `wlan_logging_th` | nice +5, efficiency core affinity, I/O priority class 3 |

---

## Requirements

- **Root manager:** Magisk v20.4+, KernelSU v0.6.6+, or APatch
- **Android:** 8.0 (API 26) or higher recommended
- **Architecture:** ARM64 (arm64-v8a) preferred; armeabi-v7a, x86, x86_64 also supported

---

## Installation

1. Download the latest `CoreTaskOptimizer.zip` from the [Releases](https://github.com/c0d3h01/CoreTaskOptimizer/releases) page
2. Open your root manager (Magisk / KernelSU / APatch)
3. Navigate to **Modules → Install from storage**
4. Select the downloaded zip and flash it
5. Reboot your device

The module runs automatically approximately 30 seconds after boot completes.

---

## Verifying Operation

After the first reboot, check the main log to confirm the module ran successfully:

```sh
cat /data/adb/modules/task_optimizer/logs/main.log
```

A successful run ends with a summary line similar to:

```
[2025-01-01 12:00:30] Operations: 84 | Success: 81 | Failed: 3
[2025-01-01 12:00:30] === System Optimization Completed ===
```

If the binary encountered errors, check:

```sh
cat /data/adb/modules/task_optimizer/logs/error.log
```

Log files are automatically rotated at 1 MB to prevent unbounded growth.

---

## Build from Source

### Prerequisites

- Android NDK r26c or later
- CMake 3.18.1+
- Ninja

### Building All ABIs

```sh
export ANDROID_NDK=/path/to/your/ndk
chmod +x build.sh
./build.sh all
```

Binaries are output to `bin/<abi>/task_optimizer`.

### Building a Specific ABI

```sh
./build.sh arm64-v8a
./build.sh armeabi-v7a
./build.sh x86
./build.sh x86_64
```

### Build Options

| Variable | Default | Description |
|---|---|---|
| `BUILD_TYPE` | `Release` | `Release` or `Debug` |
| `ANDROID_PLATFORM` | `android-21` | Minimum Android API level |
| `ANDROID_NDK` | *(required)* | Path to NDK root directory |

Release builds apply `-O3 -flto -ffunction-sections -fdata-sections` and strip the final binary.

---

## Safety Notice

This module modifies kernel scheduling parameters for running processes. While the changes are non-persistent and reset on reboot, please be aware of the following:

- **Test on non-production devices first.** Applying `SCHED_FIFO` to incorrect processes can cause system hangs.
- **Some devices may behave unexpectedly.** SoCs with custom scheduler patches (e.g., vendor-modified EAS) may not respond as expected to affinity changes.
- **Root access required.** This module will not install or function without a supported root manager.
- The module does not modify any system partitions. Uninstalling via your root manager fully removes all effects.

---

## Contributing

Contributions are welcome. Please open an issue before submitting a pull request for non-trivial changes so the approach can be discussed first.

- Follow the existing code style (C++17, no exceptions in hot paths)
- Test on at least one physical device before submitting
- Update `common/install.sh` version strings when bumping the version

---

## License

This project is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE) for the full text.

---

## Acknowledgements

**MMT Extended** by [Zackptg5](https://github.com/Zackptg5) — the Magisk module framework used as the foundation for the installer scripts.

The **Magisk**, **KernelSU**, and **APatch** projects and their communities — for building and maintaining the root management ecosystem that makes modules like this possible.
