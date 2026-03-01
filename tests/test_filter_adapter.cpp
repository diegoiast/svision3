#include <catch2/catch_test_macros.hpp>
#include "toolkit/list_view.hpp"
#include "toolkit/stopwatch.hpp"
#include <thread>

using namespace toolkit;

static auto make_source() {
    return std::make_shared<StringListAdapter>(std::vector<std::string>{
        "Apple", "Banana", "Cherry", "Apricot", "Blueberry"
    });
}

TEST_CASE("FilterAdapter shows all when no filter", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    REQUIRE(fa.count() == 5);
    REQUIRE(fa.text_at(0) == "Apple");
    REQUIRE(fa.text_at(4) == "Blueberry");
}

TEST_CASE("FilterAdapter filters by substring", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    fa.set_filter("an");
    REQUIRE(fa.count() == 1);
    REQUIRE(fa.text_at(0) == "Banana");
}

TEST_CASE("FilterAdapter case insensitive", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    fa.set_filter("AP");
    REQUIRE(fa.count() == 2);
    REQUIRE(fa.text_at(0) == "Apple");
    REQUIRE(fa.text_at(1) == "Apricot");
}

TEST_CASE("FilterAdapter empty result", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    fa.set_filter("xyz");
    REQUIRE(fa.count() == 0);
    REQUIRE(fa.text_at(0).empty());
}

TEST_CASE("FilterAdapter clear filter restores all", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    fa.set_filter("cherry");
    REQUIRE(fa.count() == 1);
    fa.set_filter("");
    REQUIRE(fa.count() == 5);
}

TEST_CASE("FilterAdapter source_index maps correctly", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    fa.set_filter("b");
    REQUIRE(fa.count() == 2);
    REQUIRE(fa.text_at(0) == "Banana");
    REQUIRE(fa.source_index(0) == 1);
    REQUIRE(fa.text_at(1) == "Blueberry");
    REQUIRE(fa.source_index(1) == 4);
}

TEST_CASE("FilterAdapter source_index out of range", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    REQUIRE(fa.source_index(-1) == -1);
    REQUIRE(fa.source_index(100) == -1);
}

TEST_CASE("FilterAdapter on_data_changed fires on filter change", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    int count = 0;
    fa.on_data_changed = [&] { count++; };
    fa.set_filter("a");
    REQUIRE(count == 1);
    fa.set_filter("ap");
    REQUIRE(count == 2);
}

TEST_CASE("FilterAdapter updates when source changes", "[filter]") {
    auto src = make_source();
    auto fa = std::make_shared<FilterAdapter>(src);
    fa->set_filter("b");
    REQUIRE(fa->count() == 2);

    src->append("Boysenberry");
    REQUIRE(fa->count() == 3);
}

TEST_CASE("FilterAdapter with null source", "[filter]") {
    FilterAdapter fa(nullptr);
    REQUIRE(fa.count() == 0);
    fa.set_filter("test");
    REQUIRE(fa.count() == 0);
}

TEST_CASE("FilterAdapter filter getter", "[filter]") {
    auto src = make_source();
    FilterAdapter fa(src);
    REQUIRE(fa.filter().empty());
    fa.set_filter("hello");
    REQUIRE(fa.filter() == "hello");
}

TEST_CASE("FilterAdapter simulated delay default is 0", "[filter]") {
    auto src = make_source();
    auto fa = std::make_shared<FilterAdapter>(src);
    fa->set_filter("cherry");
    REQUIRE(fa->count() == 1);
}

TEST_CASE("Stopwatch measures time", "[stopwatch]") {
    Stopwatch sw;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(sw.elapsed_ms() >= 15.0);
    REQUIRE(sw.elapsed_sec() < 1.0);
}

TEST_CASE("Stopwatch reset", "[stopwatch]") {
    Stopwatch sw;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sw.reset();
    REQUIRE(sw.elapsed_ms() < 15.0);
}

TEST_CASE("ScopedTimer compiles and runs", "[stopwatch]") {
    { ScopedTimer t("test-timer"); }
}
