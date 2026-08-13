#include "svision3/utf8.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <random>
#include <string>

using namespace svision3;

// ── html_escape unit tests ────────────────────────────────────────────────────

TEST_CASE("html_escape: empty string", "[utf8][html_escape]") {
    REQUIRE(html_escape("") == "");
}

TEST_CASE("html_escape: plain text passes through unchanged", "[utf8][html_escape]") {
    REQUIRE(html_escape("Hello, world!") == "Hello, world!");
}

TEST_CASE("html_escape: ampersand", "[utf8][html_escape]") {
    REQUIRE(html_escape("a&b") == "a&amp;b");
    REQUIRE(html_escape("&&") == "&amp;&amp;");
}

TEST_CASE("html_escape: less-than", "[utf8][html_escape]") {
    REQUIRE(html_escape("a<b") == "a&lt;b");
    REQUIRE(html_escape("<tag>") == "&lt;tag&gt;");
}

TEST_CASE("html_escape: greater-than", "[utf8][html_escape]") {
    REQUIRE(html_escape("a>b") == "a&gt;b");
}

TEST_CASE("html_escape: double-quote", "[utf8][html_escape]") {
    REQUIRE(html_escape("say \"hi\"") == "say &quot;hi&quot;");
}

TEST_CASE("html_escape: newline becomes <br>", "[utf8][html_escape]") {
    REQUIRE(html_escape("line1\nline2") == "line1<br>line2");
    REQUIRE(html_escape("\n") == "<br>");
}

TEST_CASE("html_escape: mixed special characters", "[utf8][html_escape]") {
    REQUIRE(html_escape("<a href=\"x&y\">") == "&lt;a href=&quot;x&amp;y&quot;&gt;");
}

TEST_CASE("html_escape: already-escaped text is double-escaped", "[utf8][html_escape]") {
    // html_escape is not idempotent — that's correct behaviour
    REQUIRE(html_escape("&amp;") == "&amp;amp;");
    REQUIRE(html_escape("&lt;") == "&amp;lt;");
}

TEST_CASE("html_escape: non-ASCII UTF-8 passes through unchanged", "[utf8][html_escape]") {
    std::string utf8 = "Héllo wörld — 日本語";
    REQUIRE(html_escape(utf8) == utf8);
}

TEST_CASE("html_escape: only special characters", "[utf8][html_escape]") {
    REQUIRE(html_escape("<>&\"") == "&lt;&gt;&amp;&quot;");
}

// ── property-based fuzzing ────────────────────────────────────────────────────

TEST_CASE("html_escape fuzz: random printable ASCII", "[utf8][html_escape][fuzz]") {
    std::mt19937 rng(0xDEADBEEF);
    std::uniform_int_distribution<int> len_dist(0, 256);
    // All printable ASCII + a few special chars
    std::string charset;
    for (char c = 0x20; c < 0x7F; ++c) {
        charset += c;
    }
    charset += '\n';

    std::uniform_int_distribution<size_t> char_dist(0, charset.size() - 1);

    for (int trial = 0; trial < 10000; ++trial) {
        int len = len_dist(rng);
        std::string input;
        input.reserve(len);
        for (int i = 0; i < len; ++i) {
            input += charset[char_dist(rng)];
        }
        auto out = html_escape(input);
        REQUIRE(is_html_escaped(out));
    }
}

TEST_CASE("html_escape fuzz: random bytes (binary)", "[utf8][html_escape][fuzz]") {
    std::mt19937 rng(0xCAFEBABE);
    std::uniform_int_distribution<int> len_dist(0, 128);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    for (int trial = 0; trial < 5000; ++trial) {
        int len = len_dist(rng);
        std::string input;
        input.reserve(len);
        for (int i = 0; i < len; ++i) {
            input += static_cast<char>(byte_dist(rng));
        }
        auto out = html_escape(input);
        REQUIRE(is_html_escaped(out));
    }
}

TEST_CASE("html_escape fuzz: strings of only special characters", "[utf8][html_escape][fuzz]") {
    std::mt19937 rng(0xBEEFCAFE);
    std::string specials = "<>&\"\n";
    std::uniform_int_distribution<int> len_dist(1, 64);
    std::uniform_int_distribution<size_t> idx_dist(0, specials.size() - 1);

    for (int trial = 0; trial < 5000; ++trial) {
        int len = len_dist(rng);
        std::string input;
        for (int i = 0; i < len; ++i) {
            input += specials[idx_dist(rng)];
        }
        auto out = html_escape(input);
        REQUIRE(is_html_escaped(out));
    }
}
