# Core Task Optimizer: Native C++ Android Performance Module

[![Android Build Test](https://github.com/c0d3h01/CoreTaskOptimizer/actions/workflows/checks.yml/badge.svg?branch=main)](https://github.com/c0d3h01/CoreTaskOptimizer/actions/workflows/checks.yml)

**A high-performance Magisk module written in C++17 that optimizes Android system processes via direct Linux syscalls.**

Unlike shell-based optimizers, **Core Task Optimizer** runs as a native binary, offering nanosecond-level efficiency in adjusting CPU affinity, scheduling policies (`SCHED_FIFO`), and I/O priorities to eliminate UI lag and reduce touch latency.

---

## ⚡ Core Features

* **🚀 Native C++ Architecture:** Built with C++17 for minimal overhead and thread-safe execution.
* **🧠 Auto-Adaptive CPU Topology:** Dynamically detects Performance vs. Efficiency cores by parsing `/sys/devices/system/cpu` frequencies. No hardcoded core masks.
* **🎮 GPU & Touch Real-Time Mode:** Forces `SCHED_FIFO` priority on critical threads like `kgsl_worker_thread` (Adreno GPU) and `fts_wq` (Touch Panels).
* **🛡️ Syscall-Level Optimization:** Bypasses standard shell commands to invoke `sched_setaffinity`, `setpriority`, and `ioprio_set` directly via the kernel interface.
* **🔋 Intelligent I/O Throttling:** automatically lowers I/O priority for background logging and garbage collection tasks (`f2fs_gc`) to prevent disk contention.

---

## 🛠️ Technical Implementation

The optimizer uses a custom `SyscallOptimizer` class to apply changes safely and effectively:

### 1. High Priority System Tasks
**Target:** `system_server`, `surfaceflinger`, `zygote`, `composer`
* **Action:** Sets **Nice -10** and locks to **Performance Cores**.
* **Result:** Instant app launches and smoother UI rendering.

### 2. Real-Time (RT) Latency Reduction
**Target:** `kgsl_worker_thread` (GPU), `crtc_commit` (Display), `nvt_ts_work` (Touch Input)
* **Action:** Applies **SCHED_FIFO** (Real-Time Scheduling) with Priority 50.
* **Result:** Eliminates micro-stutters in games and reduces input delay.

### 3. Background Suppression
**Target:** `f2fs_gc`, `wlan_logging_th`
* **Action:** Sets **Nice 5**, restricts to **Efficiency Cores**, and lowers **I/O Class to 3**.
* **Result:** Prevents background maintenance from slowing down your active app.

---

## 📊 Logging & Transparency

The module includes a thread-safe `Logger` with automatic rotation to ensure transparency without filling your storage.

* **Main Log:** `/data/adb/modules/task_optimizer/logs/main.log`
* **Error Log:** `/data/adb/modules/task_optimizer/logs/error.log`

---

## 📋 Requirements

* **Root Access:** Magisk, KernelSU, or APatch.
* **Android Version:** Android 8.0+ (Oreo and newer).
* **Architecture:** ARM64 (Snapdragon, MediaTek, Exynos, Tensor).

---

## 📥 Installation

1.  Download the latest ZIP from [Releases](#).
2.  Install via Magisk/KernelSU Manager.
3.  Reboot.
4.  Check the logs at the path above to verify successful optimizations.

---

> [!NOTE]
> **Safety First:** This module uses a `Sanitizer` class to validate PIDs and prevent regex injection. It is designed to be fail-safe; if a syscall fails, it logs the error and continues without crashing the system.

> [!CAUTION]
> **Disclaimer:** While thoroughly tested, modifying kernel scheduling parameters always carries a slight risk. I am not responsible for any software instability.
