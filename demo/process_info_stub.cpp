// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Placeholder for platforms without a process_info implementation yet (see
// process_info.hpp). Linux and Windows have real backends -- process_info_linux.cpp
// and process_info_win32.cpp -- so in practice this is the macOS build, until
// someone writes a process_info_macos.cpp (getifaddrs/sysctl for the network
// counters, proc_listpids + proc_pidinfo for the process list). Wiring one in is
// a CMakeLists.txt edit; task_manager.cpp needs no changes.
//
// Deliberately free of any platform code, including #ifdefs: everything here
// returns the documented "no implementation" value, and each platform's real
// behaviour lives in its own file rather than accumulating here.
#include "process_info.hpp"

namespace process_info {

std::vector<ProcessRow> list_processes(std::unordered_map<int, uint64_t> &, float) { return {}; }

ProcessDetail get_process_detail(int) { return ProcessDetail{}; }

std::string format_tooltip(ProcessDetail const &) { return {}; }

uint64_t total_network_bytes() { return 0; }

std::vector<SignalAction> available_signals() { return {}; }

bool send_signal(int, int) { return false; }

} // namespace process_info
