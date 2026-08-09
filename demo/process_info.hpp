// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Platform-abstracted process/network enumeration for the task_manager demo.
// hwinfo (see task_manager.cpp) covers CPU/RAM monitoring but has no process
// listing or network I/O counters at all.
namespace process_info {

struct ProcessRow {
    int pid;
    std::string name;
    float cpu_percent;
    uint64_t rss_kb;
    int threads;
    std::string state;
};

struct ProcessDetail {
    int pid = 0;
    std::string name;
    std::string exe_path = "(unavailable)";
    std::string cmdline = "(unavailable)";
    std::string user = "unknown";
    std::string parent = "none";
    int priority = 0;
    int nice = 0;
    uint64_t vm_size_kb = 0;
    uint64_t vm_peak_kb = 0;
    uint64_t vm_swap_kb = 0;
    std::string started = "unknown";
};

// Snapshots every running process. `prev_ticks` carries each PID's cumulative
// CPU-time ticks from the previous call so CPU% can be computed as a delta
// over `elapsed_sec`; it is updated in place for the next call. Returns an
// empty list on platforms without an implementation yet.
std::vector<ProcessRow> list_processes(std::unordered_map<int, uint64_t> &prev_ticks,
                                       float elapsed_sec);

// Best-effort extra detail for a single process (used for the Processes tab's
// hover tooltip). Fields the platform can't provide are left at their
// ProcessDetail defaults.
ProcessDetail get_process_detail(int pid);

// Markdown-formatted summary of a ProcessDetail, for use with
// TableView::set_row_markdown_tooltip_provider().
std::string format_tooltip(ProcessDetail const &detail);

// Sum of bytes sent+received across all (non-loopback) network interfaces
// since boot -- monotonically increasing; callers diff successive readings to
// get a rate. Returns 0 on platforms without an implementation yet.
uint64_t total_network_bytes();

// A single "send signal"-like action, as this platform actually supports it --
// POSIX's full signal(7) list on Linux, TerminateProcess/GenerateConsoleCtrlEvent
// on Windows (there's no cross-process POSIX signal delivery there at all, so
// the two lists don't correspond 1:1). `value` is an opaque platform-specific
// token to pass back into send_signal(); callers should not interpret it.
struct SignalAction {
    std::string name;
    int value;
};

// Every signal-like action this platform supports, in menu order. Empty on
// platforms without an implementation yet.
std::vector<SignalAction> available_signals();

// Best-effort: delivers the action identified by `value` (from
// available_signals()) to `pid`. Returns false on failure (no such process,
// permission denied, ...) or if this platform has no implementation.
bool send_signal(int pid, int value);

} // namespace process_info
