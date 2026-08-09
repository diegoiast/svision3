// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Windows implementation of demo/process_info.hpp -- the counterpart to
// process_info_linux.cpp. Enumeration is Toolhelp32 (CreateToolhelp32Snapshot),
// per-process CPU/memory come from GetProcessTimes/GetProcessMemoryInfo, and
// the network counters from GetIfTable2.

#include "process_info.hpp"
#include "toolkit/win32/win32_utils.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <ctime>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// clang-format off
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h> // GetIfTable2, MIB_IF_TABLE2
#include <psapi.h>    // GetProcessMemoryInfo
#include <tlhelp32.h> // CreateToolhelp32Snapshot
// clang-format on

namespace process_info {

namespace {

// Every wide string here comes from a fixed-size Win32 buffer (szExeFile,
// LookupAccountSidW's out params, ...), i.e. NUL-terminated rather than
// counted, so this adapts them to toolkit::wide_to_utf8's string_view.
std::string to_utf8(wchar_t const *w) {
    return w ? toolkit::wide_to_utf8(std::wstring_view{w}) : std::string{};
}

// FILETIME is a count of 100-nanosecond intervals. Process CPU times use it as
// a plain duration (not an absolute date), so the two halves just recombine
// into a 64-bit tick count.
uint64_t filetime_ticks(FILETIME const &ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

constexpr uint64_t kTicksPerSecond = 10'000'000ull; // 100ns intervals

HANDLE open_for_query(int pid) {
    return OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE,
                       static_cast<DWORD>(pid));
}

// "Efficiency mode" in the Windows 11 Task Manager: EcoQoS, i.e. the process
// has opted (or been put) into throttled execution speed, which parks it on
// efficiency cores. This is the closest thing Windows has to Linux's process
// state, and it is one extra call on a handle we already hold.
//
// GetProcessInformation is resolved at runtime rather than linked: it only
// exists from Windows 10 2004 (build 19041), and binding to it directly would
// make the whole demo fail to start on anything older.
bool is_efficiency_mode(HANDLE process) {
    using GetProcessInformationFn =
        BOOL(WINAPI *)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    static auto const fn = reinterpret_cast<GetProcessInformationFn>(reinterpret_cast<void *>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetProcessInformation")));
    if (!fn) {
        return false;
    }

    auto state = PROCESS_POWER_THROTTLING_STATE{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    if (!fn(process, ProcessPowerThrottling, &state, sizeof(state))) {
        return false;
    }
    // ControlMask says which policies the process manages explicitly, StateMask
    // whether each is on. Throttling is only actually in effect when the
    // EXECUTION_SPEED bit is set in both.
    return (state.ControlMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) != 0 &&
           (state.StateMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) != 0;
}

struct Handle {
    HANDLE h = nullptr;
    explicit Handle(HANDLE handle) : h(handle) {}
    ~Handle() {
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
    Handle(Handle const &) = delete;
    Handle &operator=(Handle const &) = delete;
    explicit operator bool() const { return h && h != INVALID_HANDLE_VALUE; }
};

std::string format_filetime_local(FILETIME const &ft) {
    auto st = SYSTEMTIME{};
    auto local = FILETIME{};
    if (!FileTimeToLocalFileTime(&ft, &local) || !FileTimeToSystemTime(&local, &st)) {
        return "unknown";
    }
    return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", st.wYear, st.wMonth, st.wDay,
                       st.wHour, st.wMinute, st.wSecond);
}

std::string priority_class_name(DWORD cls) {
    switch (cls) {
    case IDLE_PRIORITY_CLASS:
        return "Idle";
    case BELOW_NORMAL_PRIORITY_CLASS:
        return "Below Normal";
    case NORMAL_PRIORITY_CLASS:
        return "Normal";
    case ABOVE_NORMAL_PRIORITY_CLASS:
        return "Above Normal";
    case HIGH_PRIORITY_CLASS:
        return "High";
    case REALTIME_PRIORITY_CLASS:
        return "Realtime";
    default:
        return "Unknown";
    }
}

std::string process_name_for(int pid) {
    auto snapshot = Handle{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
        return "?";
    }
    auto entry = PROCESSENTRY32W{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot.h, &entry)) {
        do {
            if (static_cast<int>(entry.th32ProcessID) == pid) {
                return to_utf8(entry.szExeFile);
            }
        } while (Process32NextW(snapshot.h, &entry));
    }
    return "?";
}

// The account the process runs as, as DOMAIN\user. Opening the token requires
// rights we often don't have for other users' processes, so this is very much
// best-effort -- the header allows leaving the field at its default.
std::string token_user_name(HANDLE process) {
    auto raw_token = HANDLE{};
    if (!OpenProcessToken(process, TOKEN_QUERY, &raw_token)) {
        return "unknown";
    }
    auto token = Handle{raw_token};

    auto needed = DWORD{0};
    GetTokenInformation(token.h, TokenUser, nullptr, 0, &needed);
    if (needed == 0) {
        return "unknown";
    }
    auto buffer = std::vector<unsigned char>(needed);
    if (!GetTokenInformation(token.h, TokenUser, buffer.data(), needed, &needed)) {
        return "unknown";
    }

    auto const *user = reinterpret_cast<TOKEN_USER const *>(buffer.data());
    auto name = std::array<wchar_t, 256>{};
    auto domain = std::array<wchar_t, 256>{};
    auto name_len = static_cast<DWORD>(name.size());
    auto domain_len = static_cast<DWORD>(domain.size());
    auto use = SID_NAME_USE{};
    if (!LookupAccountSidW(nullptr, user->User.Sid, name.data(), &name_len, domain.data(),
                           &domain_len, &use)) {
        return "unknown";
    }
    auto domain_str = to_utf8(domain.data());
    auto name_str = to_utf8(name.data());
    return domain_str.empty() ? name_str : domain_str + "\\" + name_str;
}

} // namespace

std::vector<ProcessRow> list_processes(std::unordered_map<int, uint64_t> &prev_ticks,
                                       float elapsed_sec) {
    auto rows = std::vector<ProcessRow>{};
    auto snapshot = Handle{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
        return rows;
    }

    auto entry = PROCESSENTRY32W{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.h, &entry)) {
        return rows;
    }

    auto new_prev = std::unordered_map<int, uint64_t>{};
    do {
        auto pid = static_cast<int>(entry.th32ProcessID);
        auto row = ProcessRow{};
        row.pid = pid;
        row.name = to_utf8(entry.szExeFile);
        row.threads = static_cast<int>(entry.cntThreads);

        // Windows has nothing matching Linux's R/S/D/Z per-process state, and a
        // real "Suspended" answer would mean walking every thread of every
        // process on a 2-second refresh. Efficiency mode is the one distinction
        // it does expose cheaply, so that is what this column reports.
        row.state = "Running";

        auto process = Handle{open_for_query(pid)};
        if (process) {
            if (is_efficiency_mode(process.h)) {
                row.state = "Running (efficiency)";
            }

            auto creation = FILETIME{};
            auto exit_time = FILETIME{};
            auto kernel = FILETIME{};
            auto user = FILETIME{};
            if (GetProcessTimes(process.h, &creation, &exit_time, &kernel, &user)) {
                auto total = filetime_ticks(kernel) + filetime_ticks(user);
                if (auto it = prev_ticks.find(pid);
                    it != prev_ticks.end() && total >= it->second && elapsed_sec > 0) {
                    auto delta = total - it->second;
                    row.cpu_percent =
                        (static_cast<float>(delta) / static_cast<float>(kTicksPerSecond)) /
                        elapsed_sec * 100.0f;
                }
                new_prev[pid] = total;
            }

            auto counters = PROCESS_MEMORY_COUNTERS{};
            counters.cb = sizeof(counters);
            if (GetProcessMemoryInfo(process.h, &counters, sizeof(counters))) {
                row.rss_kb = static_cast<uint64_t>(counters.WorkingSetSize) / 1024;
            }
        }

        rows.push_back(std::move(row));
    } while (Process32NextW(snapshot.h, &entry));

    prev_ticks = std::move(new_prev);
    std::sort(rows.begin(), rows.end(),
              [](auto const &a, auto const &b) { return a.cpu_percent > b.cpu_percent; });
    return rows;
}

ProcessDetail get_process_detail(int pid) {
    auto detail = ProcessDetail{};
    detail.pid = pid;
    detail.name = process_name_for(pid);

    {
        auto snapshot = Handle{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
        if (snapshot) {
            auto entry = PROCESSENTRY32W{};
            entry.dwSize = sizeof(entry);
            if (Process32FirstW(snapshot.h, &entry)) {
                do {
                    if (static_cast<int>(entry.th32ProcessID) == pid) {
                        auto ppid = static_cast<int>(entry.th32ParentProcessID);
                        if (ppid > 0) {
                            detail.parent = fmt::format("{} ({})", process_name_for(ppid), ppid);
                        }
                        break;
                    }
                } while (Process32NextW(snapshot.h, &entry));
            }
        }
    }

    auto process = Handle{open_for_query(pid)};
    if (!process) {
        return detail;
    }

    auto path = std::array<wchar_t, MAX_PATH * 4>{};
    auto path_len = static_cast<DWORD>(path.size());
    if (QueryFullProcessImageNameW(process.h, 0, path.data(), &path_len)) {
        detail.exe_path = to_utf8(path.data());
    }

    detail.user = token_user_name(process.h);
    detail.priority = static_cast<int>(GetPriorityClass(process.h));

    auto counters = PROCESS_MEMORY_COUNTERS_EX{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(process.h, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                             sizeof(counters))) {
        detail.vm_size_kb = static_cast<uint64_t>(counters.PrivateUsage) / 1024;
        detail.vm_peak_kb = static_cast<uint64_t>(counters.PeakPagefileUsage) / 1024;
        // No per-process swap counter exists on Windows; leave vm_swap_kb at 0.
    }

    auto creation = FILETIME{};
    auto exit_time = FILETIME{};
    auto kernel = FILETIME{};
    auto user = FILETIME{};
    if (GetProcessTimes(process.h, &creation, &exit_time, &kernel, &user)) {
        detail.started = format_filetime_local(creation);
    }

    return detail;
}

std::string format_tooltip(ProcessDetail const &d) {
    return fmt::format("**{}**  (pid {})\n\n"
                       "- **Exe:** {}\n"
                       "- **User:** {}\n"
                       "- **Parent:** {}\n"
                       "- **Priority:** {}\n"
                       "- **Memory:** {:.1f} MB commit (peak {:.1f} MB)\n"
                       "- **Started:** {}\n",
                       d.name, d.pid, d.exe_path, d.user, d.parent,
                       priority_class_name(static_cast<DWORD>(d.priority)),
                       static_cast<float>(d.vm_size_kb) / 1024.0f,
                       static_cast<float>(d.vm_peak_kb) / 1024.0f, d.started);
}

uint64_t total_network_bytes() {
    MIB_IF_TABLE2 *table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table) {
        return 0;
    }

    auto total = uint64_t{0};
    for (auto i = ULONG{0}; i < table->NumEntries; i++) {
        auto const &row = table->Table[i];
        // Skip loopback, to match the Linux implementation's "lo" filter.
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        total += row.InOctets + row.OutOctets;
    }

    FreeMibTable(table);
    return total;
}

namespace {
// SignalAction::value is a plain int in the header (it is an opaque
// platform-specific token, so the header cannot name any one platform's enum),
// hence the casts at the boundary below.
enum class WinSignal : int { Terminate = 1, CtrlC = 2, CtrlBreak = 3 };
} // namespace

std::vector<SignalAction> available_signals() {
    return {
        {"Terminate", static_cast<int>(WinSignal::Terminate)},
        {"Ctrl+C (Interrupt)", static_cast<int>(WinSignal::CtrlC)},
        {"Ctrl+Break", static_cast<int>(WinSignal::CtrlBreak)},
    };
}

bool send_signal(int pid, int value) {
    switch (static_cast<WinSignal>(value)) {
    case WinSignal::Terminate: {
        auto process = Handle{OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid))};
        if (!process) {
            return false;
        }
        return TerminateProcess(process.h, 1) != 0;
    }
    case WinSignal::CtrlC:
        return GenerateConsoleCtrlEvent(CTRL_C_EVENT, static_cast<DWORD>(pid)) != 0;
    case WinSignal::CtrlBreak:
        return GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, static_cast<DWORD>(pid)) != 0;
    }
    return false;
}

} // namespace process_info
