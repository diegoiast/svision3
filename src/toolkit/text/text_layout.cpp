// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/text/text_layout.hpp"
#include <algorithm>
#include <cmath>

namespace toolkit::text {

TextLayout::TextLayout(std::string_view utf8, bidi::BaseDirection base, TextShaper &shaper,
                       float font_size, FontFamily font)
    : line_(bidi::BidiLine::analyze(utf8, base)) {
    auto const n = line_.char_count();
    char_left_x_.assign(n, 0.0f);
    char_right_x_.assign(n, 0.0f);
    if (n == 0) {
        return;
    }

    auto const &offsets = line_.char_offsets();
    auto cursor_x = 0.0f;
    for (auto const &run : line_.runs_visual()) {
        auto run_utf8 = utf8.substr(run.start, run.length);
        auto advances = shaper.shape_run(run_utf8, run.rtl(), font_size, font);

        VisualRun vrun;
        vrun.byte_start = run.start;
        vrun.byte_length = run.length;
        vrun.rtl = run.rtl();
        vrun.x = cursor_x;

        auto local_x = 0.0f;
        for (auto const &ca : advances) {
            auto abs_offset = run.start + ca.byte_offset;
            auto it = std::lower_bound(offsets.begin(), offsets.end(), abs_offset);
            if (it == offsets.end() || *it != abs_offset) {
                continue; // shaper returned an offset that isn't a character boundary; skip it
            }
            auto c = static_cast<size_t>(it - offsets.begin());
            if (c >= n) {
                continue;
            }
            char_left_x_[c] = cursor_x + local_x;
            char_right_x_[c] = cursor_x + local_x + ca.advance;
            local_x += ca.advance;
        }

        vrun.width = local_x;
        cursor_x += local_x;
        runs_.push_back(vrun);
    }

    total_width_ = cursor_x;
}

float TextLayout::caret_x(size_t byte_pos) const {
    auto const n = line_.char_count();
    if (n == 0) {
        return 0.0f;
    }

    auto const &offsets = line_.char_offsets();
    auto const &levels = line_.levels();

    // Find the character `c` whose start offset equals byte_pos (c == n
    // means byte_pos is the end of the string). Mirrors bidi::BidiLine::caret_rank.
    size_t c = 0;
    while (c < n && offsets[c] < byte_pos) {
        ++c;
    }

    if (c < n) {
        bool ltr = (levels[c] & 1u) == 0;
        return ltr ? char_left_x_[c] : char_right_x_[c];
    }

    auto const last = n - 1;
    bool ltr = (levels[last] & 1u) == 0;
    return ltr ? char_right_x_[last] : char_left_x_[last];
}

size_t TextLayout::index_from_x(float x) const {
    auto const n = line_.char_count();
    auto const &offsets = line_.char_offsets();
    if (n == 0) {
        return 0;
    }

    auto best_b = 0;
    auto best_dist = std::fabs(caret_x(offsets[0]) - x);
    for (size_t b = 1; b <= n; ++b) {
        float dist = std::fabs(caret_x(offsets[b]) - x);
        if (dist < best_dist) {
            best_dist = dist;
            best_b = b;
        }
    }
    return offsets[best_b];
}

std::vector<Rect> TextLayout::selection_rects(size_t a, size_t b) const {
    std::vector<Rect> rects;
    if (a > b) {
        std::swap(a, b);
    }
    auto const n = line_.char_count();
    if (n == 0 || a >= b) {
        return rects;
    }

    auto const &offsets = line_.char_offsets();
    auto const &logical_to_visual = line_.logical_to_visual();

    std::vector<size_t> chars;
    for (size_t c = 0; c < n; ++c) {
        if (offsets[c] >= a && offsets[c] < b) {
            chars.push_back(c);
        }
    }
    if (chars.empty()) {
        return rects;
    }

    std::sort(chars.begin(), chars.end(), [&logical_to_visual](size_t x, size_t y) {
        return logical_to_visual[x] < logical_to_visual[y];
    });

    auto run_left = char_left_x_[chars[0]];
    auto run_right = char_right_x_[chars[0]];
    auto prev_slot = logical_to_visual[chars[0]];
    for (size_t i = 1; i < chars.size(); ++i) {
        auto c = chars[i];
        auto slot = logical_to_visual[c];
        if (slot == prev_slot + 1) {
            run_right = char_right_x_[c];
        } else {
            rects.push_back(Rect{run_left, 0.0f, run_right - run_left, 0.0f});
            run_left = char_left_x_[c];
            run_right = char_right_x_[c];
        }
        prev_slot = slot;
    }
    rects.push_back(Rect{run_left, 0.0f, run_right - run_left, 0.0f});
    return rects;
}

} // namespace toolkit::text
