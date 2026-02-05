# Core Task Optimizer

[![Android Build Test](https://github.com/c0d3h01/CoreTaskOptimizer/actions/workflows/checks.yml/badge.svg?branch=main)](https://github.com/c0d3h01/CoreTaskOptimizer/actions/workflows/checks.yml)
[![License](https://img.shields.io/badge/License-Custom-blue.svg)](LICENSE)

Core Task Optimizer is a native C++ Android root module built for Magisk, KernelSU, and APatch. It improves system responsiveness by applying low level CPU affinity, scheduler, and I/O priority policies to critical system tasks using direct Linux syscalls.

## Why This Module

- Focused on system level latency and UI smoothness
- Uses native C++ syscalls instead of shell wrappers
- Applies policies at the thread level for precision
- One shot run at boot for minimal overhead

## How It Works

- Matches selected process names from `/proc/<pid>/comm`
- Applies policies to every thread in `/proc/<pid>/task`
- Groups include system critical, real time, and background maintenance
- Logs all actions to `/data/adb/modules/task_optimizer/logs/`

## Compatibility

- Root required: Magisk, KernelSU, or APatch
- Android 8.0+ recommended
- ARM64 preferred (other ABIs supported if built)

## Quick Start

- Install the module zip from Releases
- Reboot and wait for boot completion plus 30 seconds
- Open `/data/adb/modules/task_optimizer/logs/main.log`

## Documentation

This README is the index for the full wiki. Start with Home or Overview.

- [Home](../../wiki/Home)
- [Overview](../../wiki/Overview)
- [Installation](../../wiki/Installation)
- [Usage](../../wiki/Usage)
- [How It Works](../../wiki/How-It-Works)
- [Optimization Targets](../../wiki/Optimization-Targets)
- [Logging and Diagnostics](../../wiki/Logging-and-Diagnostics)
- [Troubleshooting](../../wiki/Troubleshooting)
- [Advanced Tuning](../../wiki/Advanced-Tuning)
- [Build and Development](../../wiki/Build-and-Development)
- [FAQ](../../wiki/FAQ)

## Safety Notice

This module changes kernel scheduling behavior. Read the Safety and Limits page before using it on production devices.
