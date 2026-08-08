// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "process_info.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <pwd.h>
#include <sstream>
#include <unistd.h>

namespace process_info {

namespace {

struct ProcSample {
    std::string name;
    uint64_t utime = 0, stime = 0; // clock ticks
    uint64_t rss_kb = 0;
    int threads = 0;
    char state = '?';
};

std::string_view state_name(char c) {
    switch (c) {
    case 'R':
        return "Running";
    case 'S':
        return "Sleeping";
    case 'D':
        return "Disk Sleep";
    case 'Z':
        return "Zombie";
    case 'T':
        return "Stopped";
    case 't':
        return "Tracing";
    case 'X':
        return "Dead";
    case 'I':
        return "Idle";
    default:
        return "Unknown";
    }
}

std::optional<ProcSample> read_proc_sample(int pid) {
    std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");
    if (!stat_file) {
        return std::nullopt;
    }
    std::string line;
    std::getline(stat_file, line);

    // comm is wrapped in parens and may itself contain spaces/parens, so find
    // the outermost pair and parse everything after it positionally (see
    // proc(5): fields 4-13 are ppid..cmajflt, then utime/stime at 14/15).
    auto name_start = line.find('(');
    auto name_end = line.rfind(')');
    if (name_start == std::string::npos || name_end == std::string::npos) {
        return std::nullopt;
    }

    auto sample = ProcSample{};
    sample.name = line.substr(name_start + 1, name_end - name_start - 1);

    auto rest = std::istringstream(line.substr(name_end + 2));
    rest >> sample.state;
    uint64_t ignored = 0;
    for (auto i = 0; i < 10; i++) { // ppid, pgrp, session, tty_nr, tpgid, flags, minflt,
        rest >> ignored;            // cminflt, majflt, cmajflt
    }
    rest >> sample.utime >> sample.stime;

    std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
    std::string sline;
    while (std::getline(status_file, sline)) {
        if (sline.rfind("VmRSS:", 0) == 0) {
            std::istringstream(sline.substr(6)) >> sample.rss_kb;
        } else if (sline.rfind("Threads:", 0) == 0) {
            std::istringstream(sline.substr(8)) >> sample.threads;
        }
    }
    return sample;
}

std::vector<int> list_pids() {
    auto pids = std::vector<int>{};
    for (auto const &entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) {
            continue;
        }
        auto name = entry.path().filename().string();
        if (!name.empty() && std::all_of(name.begin(), name.end(), ::isdigit)) {
            pids.push_back(std::stoi(name));
        }
    }
    return pids;
}

std::string read_proc_exe(int pid) {
    auto buf = std::array<char, 4096>{};
    auto len = ::readlink(("/proc/" + std::to_string(pid) + "/exe").c_str(), buf.data(),
                          buf.size() - 1);
    if (len <= 0) {
        return "(unavailable)";
    }
    return std::string(buf.data(), static_cast<size_t>(len));
}

// cmdline is NUL-separated argv, not newline/space-separated -- turn the NULs
// into spaces to get a normal-looking command line.
std::string read_proc_cmdline(int pid) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary);
    auto raw = std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    std::replace(raw.begin(), raw.end(), '\0', ' ');
    while (!raw.empty() && raw.back() == ' ') {
        raw.pop_back();
    }
    return raw.empty() ? "(unavailable)" : raw;
}

std::string read_proc_comm(int pid) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/comm");
    std::string name;
    std::getline(f, name);
    return name.empty() ? "?" : name;
}

uint64_t read_boot_time_epoch() {
    std::ifstream f("/proc/stat");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("btime", 0) == 0) {
            auto btime = uint64_t{0};
            std::istringstream(line.substr(5)) >> btime;
            return btime;
        }
    }
    return 0;
}

} // namespace

std::vector<ProcessRow> list_processes(std::unordered_map<int, uint64_t> &prev_ticks,
                                       float elapsed_sec) {
    auto rows = std::vector<ProcessRow>{};
    static auto const clock_ticks_per_sec = static_cast<float>(sysconf(_SC_CLK_TCK));
    auto new_prev = std::unordered_map<int, uint64_t>{};
    for (auto pid : list_pids()) {
        auto sample = read_proc_sample(pid);
        if (!sample) {
            continue;
        }
        auto total_ticks = sample->utime + sample->stime;
        auto cpu_percent = 0.0f;
        if (auto it = prev_ticks.find(pid); it != prev_ticks.end() && total_ticks >= it->second) {
            auto delta_ticks = total_ticks - it->second;
            cpu_percent = (static_cast<float>(delta_ticks) / clock_ticks_per_sec) / elapsed_sec * 100.0f;
        }
        new_prev[pid] = total_ticks;
        rows.push_back({pid, sample->name, cpu_percent, sample->rss_kb, sample->threads,
                        std::string(state_name(sample->state))});
    }
    prev_ticks = std::move(new_prev);
    std::sort(rows.begin(), rows.end(),
             [](auto const &a, auto const &b) { return a.cpu_percent > b.cpu_percent; });
    return rows;
}

// Reads a handful of /proc files fresh on every call, which is fine since
// it's only invoked while a row is actively hovered rather than on every
// periodic refresh tick.
ProcessDetail get_process_detail(int pid) {
    auto detail = ProcessDetail{};
    detail.pid = pid;
    detail.name = read_proc_comm(pid);
    detail.exe_path = read_proc_exe(pid);
    detail.cmdline = read_proc_cmdline(pid);

    auto ppid = 0;
    auto starttime_ticks = uint64_t{0};
    {
        std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");
        std::string line;
        std::getline(stat_file, line);
        auto name_end = line.rfind(')');
        if (name_end != std::string::npos) {
            auto rest = std::istringstream(line.substr(name_end + 2));
            char state_c = '?';
            uint64_t ignored = 0;
            // proc(5) fields after "(comm)": state(3) ppid(4) pgrp(5) session(6)
            // tty_nr(7) tpgid(8) flags(9) minflt(10) cminflt(11) majflt(12)
            // cmajflt(13) utime(14) stime(15) cutime(16) cstime(17) priority(18)
            // nice(19) num_threads(20) itrealvalue(21) starttime(22).
            rest >> state_c >> ppid;
            for (auto i = 0; i < 9; i++) { // pgrp..cmajflt
                rest >> ignored;
            }
            for (auto i = 0; i < 4; i++) { // utime, stime, cutime, cstime
                rest >> ignored;
            }
            rest >> detail.priority >> detail.nice;
            rest >> ignored;         // num_threads
            rest >> ignored;         // itrealvalue
            rest >> starttime_ticks; // starttime
        }
    }

    auto uid = -1;
    {
        std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
        std::string sline;
        while (std::getline(status_file, sline)) {
            if (sline.rfind("Uid:", 0) == 0) {
                std::istringstream(sline.substr(4)) >> uid;
            } else if (sline.rfind("VmPeak:", 0) == 0) {
                std::istringstream(sline.substr(7)) >> detail.vm_peak_kb;
            } else if (sline.rfind("VmSize:", 0) == 0) {
                std::istringstream(sline.substr(7)) >> detail.vm_size_kb;
            } else if (sline.rfind("VmSwap:", 0) == 0) {
                std::istringstream(sline.substr(7)) >> detail.vm_swap_kb;
            }
        }
    }

    detail.user = std::string("uid ") + std::to_string(uid);
    if (uid >= 0) {
        if (auto *pw = getpwuid(static_cast<uid_t>(uid))) {
            detail.user = pw->pw_name;
        }
    }

    detail.parent = ppid > 0 ? fmt::format("{} ({})", read_proc_comm(ppid), ppid) : "none";

    auto boot_time = read_boot_time_epoch();
    if (boot_time > 0) {
        static auto const clock_ticks_per_sec = static_cast<float>(sysconf(_SC_CLK_TCK));
        auto start_epoch =
            static_cast<time_t>(boot_time + static_cast<uint64_t>(
                                                static_cast<float>(starttime_ticks) / clock_ticks_per_sec));
        auto tm_buf = std::tm{};
        localtime_r(&start_epoch, &tm_buf);
        auto buf = std::array<char, 32>{};
        if (std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S", &tm_buf) > 0) {
            detail.started = buf.data();
        }
    }

    return detail;
}

std::string format_tooltip(ProcessDetail const &d) {
    // Each field is its own markdown list item rather than being joined with
    // separators on one line -- list items always render on their own line,
    // whereas a single '\n' inside a paragraph is just a soft break (a space)
    // under CommonMark, so a "joined" version would collapse back to one line.
    return fmt::format(
        "**{}**  (pid {})\n\n"
        "- **Exe:** {}\n"
        "- **User:** {}\n"
        "- **Parent:** {}\n"
        "- **Priority:** {} (nice {})\n"
        "- **Memory:** {:.1f} MB (peak {:.1f} MB, swap {:.1f} MB)\n"
        "- **Started:** {}\n"
        "- **Cmd:** {}\n",
        d.name, d.pid, d.exe_path, d.user, d.parent, d.priority, d.nice,
        static_cast<float>(d.vm_size_kb) / 1024.0f, static_cast<float>(d.vm_peak_kb) / 1024.0f,
        static_cast<float>(d.vm_swap_kb) / 1024.0f, d.started, d.cmdline);
}

// The standard (non-realtime) POSIX/Linux signal set from signal(7); the
// realtime range (SIGRTMIN..SIGRTMAX) is left out since those are
// application-defined and meaningless to offer generically in a menu.
std::vector<SignalAction> available_signals() {
    return {
        {"SIGHUP (1) -- Hangup", SIGHUP},
        {"SIGINT (2) -- Interrupt", SIGINT},
        {"SIGQUIT (3) -- Quit", SIGQUIT},
        {"SIGILL (4) -- Illegal instruction", SIGILL},
        {"SIGTRAP (5) -- Trace/breakpoint trap", SIGTRAP},
        {"SIGABRT (6) -- Abort", SIGABRT},
        {"SIGBUS (7) -- Bus error", SIGBUS},
        {"SIGFPE (8) -- Floating point exception", SIGFPE},
        {"SIGKILL (9) -- Kill", SIGKILL},
        {"SIGUSR1 (10) -- User-defined 1", SIGUSR1},
        {"SIGSEGV (11) -- Segmentation fault", SIGSEGV},
        {"SIGUSR2 (12) -- User-defined 2", SIGUSR2},
        {"SIGPIPE (13) -- Broken pipe", SIGPIPE},
        {"SIGALRM (14) -- Alarm clock", SIGALRM},
        {"SIGTERM (15) -- Terminate", SIGTERM},
        {"SIGSTKFLT (16) -- Stack fault", SIGSTKFLT},
        {"SIGCHLD (17) -- Child status changed", SIGCHLD},
        {"SIGCONT (18) -- Continue", SIGCONT},
        {"SIGSTOP (19) -- Stop", SIGSTOP},
        {"SIGTSTP (20) -- Terminal stop", SIGTSTP},
        {"SIGTTIN (21) -- Background read from tty", SIGTTIN},
        {"SIGTTOU (22) -- Background write to tty", SIGTTOU},
        {"SIGURG (23) -- Urgent socket condition", SIGURG},
        {"SIGXCPU (24) -- CPU time limit exceeded", SIGXCPU},
        {"SIGXFSZ (25) -- File size limit exceeded", SIGXFSZ},
        {"SIGVTALRM (26) -- Virtual alarm clock", SIGVTALRM},
        {"SIGPROF (27) -- Profiling timer expired", SIGPROF},
        {"SIGWINCH (28) -- Window resize", SIGWINCH},
        {"SIGIO (29) -- I/O now possible", SIGIO},
        {"SIGPWR (30) -- Power failure", SIGPWR},
        {"SIGSYS (31) -- Bad system call", SIGSYS},
    };
}

bool send_signal(int pid, int value) { return ::kill(pid, value) == 0; }

uint64_t total_network_bytes() {
    std::ifstream f("/proc/net/dev");
    std::string line;
    std::getline(f, line); // header line 1
    std::getline(f, line); // header line 2

    uint64_t total = 0;
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        auto iface = line.substr(0, colon);
        iface.erase(0, iface.find_first_not_of(" \t"));
        if (iface == "lo") {
            continue;
        }

        std::istringstream fields(line.substr(colon + 1));
        uint64_t rx_bytes = 0, tx_bytes = 0, ignored = 0;
        fields >> rx_bytes;
        for (auto i = 0; i < 7; i++) {
            fields >> ignored;
        }
        fields >> tx_bytes;
        total += rx_bytes + tx_bytes;
    }
    return total;
}

} // namespace process_info
