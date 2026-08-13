// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <chrono>
#include <spdlog/spdlog.h>
#include <string>

namespace svision3 {

class Stopwatch {
  public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}

    void reset() { start_ = std::chrono::steady_clock::now(); }

    double elapsed_ms() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    double elapsed_sec() const { return elapsed_ms() / 1000.0; }

  private:
    std::chrono::steady_clock::time_point start_;
};

class ScopedTimer {
  public:
    explicit ScopedTimer(std::string label) : label_(std::move(label)) {}
    ~ScopedTimer() { spdlog::info("{}: {:.2f} ms", label_, sw_.elapsed_ms()); }

    ScopedTimer(ScopedTimer const &) = delete;
    ScopedTimer &operator=(ScopedTimer const &) = delete;

  private:
    std::string label_;
    Stopwatch sw_;
};

} // namespace svision3
