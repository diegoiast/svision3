// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Placeholder for platforms without a full process_info implementation yet
// (see process_info.hpp). Swap this out of CMakeLists.txt for a real
// process_info_win32.cpp/process_info_macos.cpp (GetIfTable2+EnumProcesses,
// or getifaddrs+SIOCGIFDATA/sysctl) to light up the Network/Processes tabs
// there; task_manager.cpp itself needs no changes.
//
// Signal delivery is the exception: unlike process enumeration, Windows'
// side of it (TerminateProcess/GenerateConsoleCtrlEvent) is small enough to
// implement directly here rather than waiting on a full process_info_win32.cpp.
#include "process_info.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace process_info {

std::vector<ProcessRow> list_processes(std::unordered_map<int, uint64_t> &, float) { return {}; }

ProcessDetail get_process_detail(int) { return ProcessDetail{}; }

std::string format_tooltip(ProcessDetail const &) { return {}; }

uint64_t total_network_bytes() { return 0; }

#ifdef _WIN32
namespace {
// There's no cross-process POSIX signal delivery on Windows at all, so these
// aren't "signals" in any real sense -- they're the closest native
// equivalents: TerminateProcess for an unconditional kill, and
// GenerateConsoleCtrlEvent for a "please exit" nudge to a console process
// group (it silently fails outside that scenario -- there's no general
// "ask any PID to exit gracefully" API on Windows).
enum WinSignalValue { kWinTerminate = 1, kWinCtrlC = 2, kWinCtrlBreak = 3 };
} // namespace

std::vector<SignalAction> available_signals() {
    return {
        {"Terminate", kWinTerminate},
        {"Ctrl+C (Interrupt)", kWinCtrlC},
        {"Ctrl+Break", kWinCtrlBreak},
    };
}

bool send_signal(int pid, int value) {
    switch (value) {
    case kWinTerminate: {
        auto *h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (!h) {
            return false;
        }
        auto ok = TerminateProcess(h, 1);
        CloseHandle(h);
        return ok != 0;
    }
    case kWinCtrlC:
        return GenerateConsoleCtrlEvent(CTRL_C_EVENT, static_cast<DWORD>(pid)) != 0;
    case kWinCtrlBreak:
        return GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, static_cast<DWORD>(pid)) != 0;
    default:
        return false;
    }
}
#else
std::vector<SignalAction> available_signals() { return {}; }

bool send_signal(int, int) { return false; }
#endif

} // namespace process_info
