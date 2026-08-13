// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/text/bidi.hpp"
#include <algorithm>

namespace svision3::bidi {

namespace {

// Bidi character classes (UAX #9 table 4), restricted to what this
// scoped-down implementation resolves. Explicit directional formatting
// characters and isolates are intentionally not modeled (documented
// limitation) and classify as ON.
enum class CharClass : uint8_t {
    L,
    R,
    AL,
    EN,
    AN,
    ES,
    ET,
    CS,
    NSM,
    B,
    S,
    WS,
    ON,
};

bool is_strong(CharClass c) { return c == CharClass::L || c == CharClass::R || c == CharClass::AL; }
bool is_neutral_or_iso(CharClass c) {
    return c == CharClass::B || c == CharClass::S || c == CharClass::WS || c == CharClass::ON;
}

CharClass classify(char32_t cp) {
    // Paragraph / segment separators.
    if (cp == U'\n' || cp == U'\r' || cp == 0x2029) {
        return CharClass::B;
    }
    if (cp == U'\t' || cp == 0x000B || cp == 0x001F) {
        return CharClass::S;
    }
    // Whitespace.
    if (cp == U' ' || cp == 0x00A0 || cp == 0x202F || cp == 0x205F || cp == 0x3000 ||
        cp == 0x000C) {
        return CharClass::WS;
    }
    if (cp >= 0x2000 && cp <= 0x200A) {
        return CharClass::WS;
    }

    // Hebrew block.
    if (cp >= 0x0591 && cp <= 0x05BD) {
        return CharClass::NSM;
    }
    if (cp == 0x05BF || cp == 0x05C1 || cp == 0x05C2 || cp == 0x05C4 || cp == 0x05C5 ||
        cp == 0x05C7) {
        return CharClass::NSM;
    }
    if (cp >= 0x0590 && cp <= 0x05FF) {
        return CharClass::R;
    }
    // Hebrew presentation forms / other RTL-only blocks.
    if (cp >= 0x07C0 && cp <= 0x085F) {
        return CharClass::R; // NKo, Samaritan, Mandaic (treated as plain R; not exhaustive)
    }
    if (cp >= 0xFB1D && cp <= 0xFB4F) {
        return CharClass::R; // Hebrew presentation forms
    }

    // Arabic combining marks.
    if ((cp >= 0x0610 && cp <= 0x061A) || (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670 ||
        (cp >= 0x06D6 && cp <= 0x06DC) || (cp >= 0x06DF && cp <= 0x06E4) || cp == 0x06E7 ||
        cp == 0x06E8 || (cp >= 0x06EA && cp <= 0x06ED)) {
        return CharClass::NSM;
    }
    // Arabic-Indic digits (AN) vs extended Arabic-Indic digits (EN).
    if (cp >= 0x0660 && cp <= 0x0669) {
        return CharClass::AN;
    }
    if (cp >= 0x06F0 && cp <= 0x06F9) {
        return CharClass::EN;
    }
    // Arabic letters.
    if ((cp >= 0x0600 && cp <= 0x06FF) || (cp >= 0x0750 && cp <= 0x077F) ||
        (cp >= 0x08A0 && cp <= 0x08FF) || (cp >= 0xFB50 && cp <= 0xFDFF) ||
        (cp >= 0xFE70 && cp <= 0xFEFF)) {
        return CharClass::AL;
    }

    // Combining diacritical marks (Latin/Cyrillic/Greek etc).
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF)) {
        return CharClass::NSM;
    }

    // European digits.
    if (cp >= U'0' && cp <= U'9') {
        return CharClass::EN;
    }
    if (cp >= 0x2070 && cp <= 0x2079) {
        return CharClass::EN; // superscript digits
    }
    if (cp >= 0x2080 && cp <= 0x2089) {
        return CharClass::EN; // subscript digits
    }

    // Common/European separators and terminators.
    switch (cp) {
    case U',':
    case U'.':
    case U':':
        return CharClass::CS;
    case U'+':
    case U'-':
    case 0x207A:
    case 0x207B:
    case 0x208A:
    case 0x208B:
        return CharClass::ES;
    case U'$':
    case U'%':
    case 0x00A2:
    case 0x00A3:
    case 0x00A4:
    case 0x00A5:
    case 0x00B0:
    case 0x2030:
    case 0x2031:
        return CharClass::ET;
    default:
        break;
    }

    // ASCII / Latin letters and most scripts not enumerated above default to L.
    // This is a deliberate simplification: every script that is not RTL above
    // (Latin, Cyrillic, Greek, CJK, Thai, Devanagari, ...) is treated as
    // strong-L, which is correct for direction resolution purposes even
    // though it is not a full UAX #9 property table.
    if ((cp >= U'A' && cp <= U'Z') || (cp >= U'a' && cp <= U'z')) {
        return CharClass::L;
    }
    if (cp < 0x20 || cp == 0x7F) {
        return CharClass::ON; // control characters
    }
    if ((cp >= 0x21 && cp <= 0x2F) || (cp >= 0x3A && cp <= 0x40) || (cp >= 0x5B && cp <= 0x60) ||
        (cp >= 0x7B && cp <= 0x7E)) {
        return CharClass::ON; // ASCII punctuation/symbols not covered above
    }
    return CharClass::L;
}

CharClass norm_for_n_rules(CharClass c) {
    // N1: EN and AN act as R for the purpose of matching neighbouring strong runs.
    return (c == CharClass::EN || c == CharClass::AN) ? CharClass::R : c;
}

struct Decoded {
    std::vector<char32_t> codepoints;
    std::vector<size_t> offsets; // size codepoints.size() + 1
};

Decoded decode(std::string_view s) {
    Decoded d;
    d.offsets.push_back(0);
    size_t pos = 0;
    while (pos < s.size()) {
        unsigned char c0 = static_cast<unsigned char>(s[pos]);
        char32_t cp;
        size_t len;
        if (c0 < 0x80) {
            cp = c0;
            len = 1;
        } else if ((c0 & 0xE0) == 0xC0 && pos + 1 < s.size()) {
            unsigned char c1 = static_cast<unsigned char>(s[pos + 1]);
            cp = (static_cast<char32_t>(c0 & 0x1F) << 6) | (c1 & 0x3F);
            len = 2;
        } else if ((c0 & 0xF0) == 0xE0 && pos + 2 < s.size()) {
            unsigned char c1 = static_cast<unsigned char>(s[pos + 1]);
            unsigned char c2 = static_cast<unsigned char>(s[pos + 2]);
            cp = (static_cast<char32_t>(c0 & 0x0F) << 12) |
                 (static_cast<char32_t>(c1 & 0x3F) << 6) | (c2 & 0x3F);
            len = 3;
        } else if ((c0 & 0xF8) == 0xF0 && pos + 3 < s.size()) {
            unsigned char c1 = static_cast<unsigned char>(s[pos + 1]);
            unsigned char c2 = static_cast<unsigned char>(s[pos + 2]);
            unsigned char c3 = static_cast<unsigned char>(s[pos + 3]);
            cp = (static_cast<char32_t>(c0 & 0x07) << 18) |
                 (static_cast<char32_t>(c1 & 0x3F) << 12) |
                 (static_cast<char32_t>(c2 & 0x3F) << 6) | (c3 & 0x3F);
            len = 4;
        } else {
            cp = 0xFFFD;
            len = 1;
        }
        d.codepoints.push_back(cp);
        pos += len;
        d.offsets.push_back(pos);
    }
    return d;
}

} // namespace

BaseDirection detect_base_direction(std::string_view utf8) {
    auto decoded = decode(utf8);
    for (auto cp : decoded.codepoints) {
        auto cls = classify(cp);
        if (cls == CharClass::L) {
            return BaseDirection::LTR;
        }
        if (cls == CharClass::R || cls == CharClass::AL) {
            return BaseDirection::RTL;
        }
    }
    return BaseDirection::LTR;
}

BidiLine BidiLine::analyze(std::string_view utf8, BaseDirection base) {
    BidiLine result;
    result.base_ = base;

    auto decoded = decode(utf8);
    auto const n = decoded.codepoints.size();
    result.offsets_ = std::move(decoded.offsets);

    if (n == 0) {
        return result;
    }

    auto const base_level = static_cast<uint8_t>(base == BaseDirection::RTL ? 1 : 0);
    auto const sos = (base == BaseDirection::RTL) ? CharClass::R : CharClass::L;

    std::vector<CharClass> orig(n);
    for (size_t i = 0; i < n; ++i) {
        orig[i] = classify(decoded.codepoints[i]);
    }
    std::vector<CharClass> types = orig;

    // ---- W1: NSM takes the type of the previous character (sos if first) ----
    {
        CharClass prev = sos;
        for (size_t i = 0; i < n; ++i) {
            if (types[i] == CharClass::NSM) {
                types[i] = prev;
            }
            prev = types[i];
        }
    }

    // ---- W2: EN -> AN if the last strong type seen (L/R/AL) was AL ----
    {
        CharClass strong = sos;
        for (size_t i = 0; i < n; ++i) {
            if (types[i] == CharClass::EN && strong == CharClass::AL) {
                types[i] = CharClass::AN;
            }
            if (is_strong(types[i])) {
                strong = types[i];
            }
        }
    }

    // ---- W3: AL -> R ----
    for (size_t i = 0; i < n; ++i) {
        if (types[i] == CharClass::AL) {
            types[i] = CharClass::R;
        }
    }

    // ---- W4: ES between two EN -> EN; CS between two equal numbers -> that type ----
    for (size_t i = 1; i + 1 < n; ++i) {
        if (types[i] == CharClass::ES && types[i - 1] == CharClass::EN &&
            types[i + 1] == CharClass::EN) {
            types[i] = CharClass::EN;
        } else if (types[i] == CharClass::CS && types[i - 1] == types[i + 1] &&
                   (types[i - 1] == CharClass::EN || types[i - 1] == CharClass::AN)) {
            types[i] = types[i - 1];
        }
    }

    // ---- W5: ET adjacent to EN -> EN (propagate along contiguous ET runs) ----
    for (size_t i = 1; i < n; ++i) {
        if (types[i] == CharClass::ET && types[i - 1] == CharClass::EN) {
            types[i] = CharClass::EN;
        }
    }
    for (size_t i = n; i-- > 1;) {
        if (types[i - 1] == CharClass::ET && types[i] == CharClass::EN) {
            types[i - 1] = CharClass::EN;
        }
    }

    // ---- W6: remaining ES, ET, CS -> ON ----
    for (size_t i = 0; i < n; ++i) {
        if (types[i] == CharClass::ES || types[i] == CharClass::ET || types[i] == CharClass::CS) {
            types[i] = CharClass::ON;
        }
    }

    // ---- W7: EN -> L if the last strong L/R type seen was L ----
    {
        CharClass strong = sos;
        for (size_t i = 0; i < n; ++i) {
            if (types[i] == CharClass::EN && strong == CharClass::L) {
                types[i] = CharClass::L;
            }
            if (types[i] == CharClass::L || types[i] == CharClass::R) {
                strong = types[i];
            }
        }
    }

    // ---- N1/N2: resolve runs of neutrals (B, S, WS, ON) ----
    // Single embedding level throughout (no explicit formatting modeled), so
    // the "embedding direction" fallback for N2 is simply the base direction.
    {
        size_t i = 0;
        while (i < n) {
            if (!is_neutral_or_iso(types[i])) {
                ++i;
                continue;
            }
            size_t j = i;
            while (j < n && is_neutral_or_iso(types[j])) {
                ++j;
            }
            // sos and eos coincide here: with no isolates modeled the whole
            // line is one isolating run sequence, so "end of sequence" is the
            // same base direction as "start of sequence" — `sos` stands in
            // for both boundaries.
            CharClass left = (i == 0) ? sos : norm_for_n_rules(types[i - 1]);
            CharClass right = (j == n) ? sos : norm_for_n_rules(types[j]);
            CharClass resolved;
            if (left == CharClass::L && right == CharClass::L) {
                resolved = CharClass::L;
            } else if (left == CharClass::R && right == CharClass::R) {
                resolved = CharClass::R;
            } else {
                resolved = sos; // N2: fall back to the (single) embedding direction
            }
            for (size_t k = i; k < j; ++k) {
                types[k] = resolved;
            }
            i = j;
        }
    }

    // ---- I1/I2: implicit levels ----
    std::vector<uint8_t> levels(n, base_level);
    for (size_t i = 0; i < n; ++i) {
        auto level = levels[i];
        auto t = types[i];
        if ((level & 1u) == 0) { // even / LTR
            if (t == CharClass::R) {
                level += 1;
            } else if (t == CharClass::AN || t == CharClass::EN) {
                level += 2;
            }
        } else { // odd / RTL
            if (t == CharClass::L || t == CharClass::EN || t == CharClass::AN) {
                level += 1;
            }
        }
        levels[i] = level;
    }

    // ---- L1: reset segment/paragraph separators and trailing whitespace ----
    for (size_t i = 0; i < n; ++i) {
        if (orig[i] == CharClass::B || orig[i] == CharClass::S) {
            levels[i] = base_level;
        }
    }
    {
        bool trailing = true;
        for (size_t i = n; i-- > 0;) {
            if (orig[i] == CharClass::WS) {
                if (trailing) {
                    levels[i] = base_level;
                }
            } else if (orig[i] == CharClass::B || orig[i] == CharClass::S) {
                trailing = true;
            } else {
                trailing = false;
            }
        }
    }

    result.levels_ = levels;

    // ---- L2: reorder into visual order (character granularity) ----
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) {
        order[i] = i;
    }
    uint8_t max_level = 0;
    for (auto l : levels) {
        max_level = std::max(max_level, l);
    }
    for (int level = max_level; level >= 1; --level) {
        size_t k = 0;
        while (k < n) {
            if (levels[order[k]] < level) {
                ++k;
                continue;
            }
            size_t run_end = k;
            while (run_end < n && levels[order[run_end]] >= level) {
                ++run_end;
            }
            std::reverse(order.begin() + static_cast<long>(k),
                         order.begin() + static_cast<long>(run_end));
            k = run_end;
        }
    }
    result.visual_to_logical_ = order;

    result.logical_to_visual_.assign(n, 0);
    for (size_t k = 0; k < n; ++k) {
        result.logical_to_visual_[order[k]] = k;
    }

    // ---- derive left-to-right visual runs from the character permutation ----
    {
        size_t k = 0;
        while (k < n) {
            auto level = levels[order[k]];
            size_t run_start_logical = order[k];
            size_t run_end_logical = order[k];
            size_t j = k + 1;
            while (j < n && levels[order[j]] == level) {
                run_start_logical = std::min(run_start_logical, order[j]);
                run_end_logical = std::max(run_end_logical, order[j]);
                ++j;
            }
            Run run;
            run.start = result.offsets_[run_start_logical];
            run.length = result.offsets_[run_end_logical + 1] - run.start;
            run.level = level;
            result.runs_visual_.push_back(run);
            k = j;
        }
    }

    return result;
}

uint8_t BidiLine::level_at(size_t byte) const {
    if (levels_.empty() || byte >= offsets_.back()) {
        return base_ == BaseDirection::RTL ? 1 : 0;
    }
    for (size_t i = 0; i + 1 < offsets_.size(); ++i) {
        if (byte >= offsets_[i] && byte < offsets_[i + 1]) {
            return levels_[i];
        }
    }
    return base_ == BaseDirection::RTL ? 1 : 0;
}

size_t BidiLine::caret_rank(size_t byte_pos) const {
    auto const n = levels_.size();
    if (n == 0) {
        return 0;
    }

    // Find the character `c` whose start offset equals byte_pos (c == n
    // means byte_pos is the end of the string, past the last character).
    size_t c = 0;
    while (c < n && offsets_[c] < byte_pos) {
        ++c;
    }

    if (c < n) {
        // Caret sits immediately before character c. For an LTR (even
        // level) character that is its visual left edge -- its own slot is
        // the count of characters to the left. For RTL it is the visual
        // right edge, so one more character (itself) is to the left.
        auto const slot = logical_to_visual_[c];
        return (levels_[c] & 1u) == 0 ? slot : slot + 1;
    }

    // Caret sits immediately after the last character. Mirror image of the
    // above: LTR's right edge counts the character itself, RTL's left edge
    // does not.
    auto const last = n - 1;
    auto const slot = logical_to_visual_[last];
    return (levels_[last] & 1u) == 0 ? slot + 1 : slot;
}

} // namespace svision3::bidi
