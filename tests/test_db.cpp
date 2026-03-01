#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "db/connection.hpp"

TEST_CASE("Open in-memory database", "[db]") {
    auto conn = db::open(":memory:");
    REQUIRE(conn != nullptr);
}

TEST_CASE("Open with sqlite: prefix", "[db]") {
    auto conn = db::open("sqlite::memory:");
    REQUIRE(conn != nullptr);
}

TEST_CASE("Create table and insert", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute(
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

    auto stmt = conn->prepare("INSERT INTO users (name, age) VALUES (?, ?)");
    stmt->bind(1, "Alice").bind(2, 30);
    stmt->execute_update();

    REQUIRE(conn->last_insert_id() == 1);
}

TEST_CASE("Query returns rows", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT)");
    conn->execute("INSERT INTO t (val) VALUES ('one')");
    conn->execute("INSERT INTO t (val) VALUES ('two')");
    conn->execute("INSERT INTO t (val) VALUES ('three')");

    auto stmt = conn->prepare("SELECT id, val FROM t ORDER BY id");
    auto result = stmt->execute();

    REQUIRE(result->column_count() == 2);
    REQUIRE(result->column_name(0) == "id");
    REQUIRE(result->column_name(1) == "val");

    REQUIRE(result->next());
    REQUIRE(result->get_int(0) == 1);
    REQUIRE(result->get_string(1) == "one");

    REQUIRE(result->next());
    REQUIRE(result->get_int(0) == 2);
    REQUIRE(result->get_string(1) == "two");

    REQUIRE(result->next());
    REQUIRE(result->get_int(0) == 3);
    REQUIRE(result->get_string(1) == "three");

    REQUIRE_FALSE(result->next());
}

TEST_CASE("Bind all types", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE types (i INT, i64 INT, d REAL, t TEXT, n INT)");

    auto stmt =
        conn->prepare("INSERT INTO types VALUES (?, ?, ?, ?, ?)");
    stmt->bind(1, 42)
        .bind(2, int64_t(9'000'000'000LL))
        .bind(3, 3.14)
        .bind(4, "hello")
        .bind_null(5);
    stmt->execute_update();

    auto q = conn->prepare("SELECT * FROM types");
    auto r = q->execute();
    REQUIRE(r->next());
    REQUIRE(r->get_int(0) == 42);
    REQUIRE(r->get_int64(1) == 9'000'000'000LL);
    REQUIRE(r->get_double(2) == Catch::Approx(3.14));
    REQUIRE(r->get_string(3) == "hello");
    REQUIRE(r->is_null(4));
}

TEST_CASE("Parameterized query", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE kv (key TEXT, val INTEGER)");
    conn->execute("INSERT INTO kv VALUES ('a', 1)");
    conn->execute("INSERT INTO kv VALUES ('b', 2)");
    conn->execute("INSERT INTO kv VALUES ('c', 3)");

    auto stmt = conn->prepare("SELECT val FROM kv WHERE key = ?");
    stmt->bind(1, "b");
    auto r = stmt->execute();
    REQUIRE(r->next());
    REQUIRE(r->get_int(0) == 2);
    REQUIRE_FALSE(r->next());
}

TEST_CASE("execute_update returns affected rows", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)");
    conn->execute("INSERT INTO t (v) VALUES (10)");
    conn->execute("INSERT INTO t (v) VALUES (20)");
    conn->execute("INSERT INTO t (v) VALUES (30)");

    auto stmt = conn->prepare("UPDATE t SET v = v + 1 WHERE v >= 20");
    int affected = stmt->execute_update();
    REQUIRE(affected == 2);
}

TEST_CASE("Statement reset and rebind", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE t (v TEXT)");

    auto stmt = conn->prepare("INSERT INTO t (v) VALUES (?)");
    stmt->bind(1, "first");
    stmt->execute_update();
    stmt->reset();
    stmt->bind(1, "second");
    stmt->execute_update();

    auto q = conn->prepare("SELECT v FROM t ORDER BY rowid");
    auto r = q->execute();
    REQUIRE(r->next());
    REQUIRE(r->get_string(0) == "first");
    REQUIRE(r->next());
    REQUIRE(r->get_string(0) == "second");
}

TEST_CASE("Transaction commit", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE t (v INTEGER)");

    conn->begin();
    conn->execute("INSERT INTO t VALUES (1)");
    conn->execute("INSERT INTO t VALUES (2)");
    conn->commit();

    auto q = conn->prepare("SELECT COUNT(*) FROM t");
    auto r = q->execute();
    REQUIRE(r->next());
    REQUIRE(r->get_int(0) == 2);
}

TEST_CASE("Transaction rollback", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE t (v INTEGER)");
    conn->execute("INSERT INTO t VALUES (1)");

    conn->begin();
    conn->execute("INSERT INTO t VALUES (2)");
    conn->execute("INSERT INTO t VALUES (3)");
    conn->rollback();

    auto q = conn->prepare("SELECT COUNT(*) FROM t");
    auto r = q->execute();
    REQUIRE(r->next());
    REQUIRE(r->get_int(0) == 1);
}

TEST_CASE("Foreign keys are enabled", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE parent (id INTEGER PRIMARY KEY)");
    conn->execute(
        "CREATE TABLE child (id INTEGER PRIMARY KEY, "
        "parent_id INTEGER REFERENCES parent(id))");
    conn->execute("INSERT INTO parent VALUES (1)");

    REQUIRE_THROWS_AS(
        conn->execute("INSERT INTO child VALUES (1, 999)"), db::Error);
}

TEST_CASE("Error on bad SQL", "[db]") {
    auto conn = db::open(":memory:");
    REQUIRE_THROWS_AS(conn->execute("NOT VALID SQL"), db::Error);
}

TEST_CASE("Error on bad prepare", "[db]") {
    auto conn = db::open(":memory:");
    REQUIRE_THROWS_AS(conn->prepare("SELECT * FROM nonexistent"), db::Error);
}

TEST_CASE("Null string returns empty", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE t (v TEXT)");
    conn->execute("INSERT INTO t VALUES (NULL)");

    auto q = conn->prepare("SELECT v FROM t");
    auto r = q->execute();
    REQUIRE(r->next());
    REQUIRE(r->is_null(0));
    REQUIRE(r->get_string(0).empty());
}

TEST_CASE("Multiple last_insert_id", "[db]") {
    auto conn = db::open(":memory:");
    conn->execute("CREATE TABLE t (id INTEGER PRIMARY KEY)");

    conn->execute("INSERT INTO t VALUES (NULL)");
    REQUIRE(conn->last_insert_id() == 1);

    conn->execute("INSERT INTO t VALUES (NULL)");
    REQUIRE(conn->last_insert_id() == 2);

    conn->execute("INSERT INTO t VALUES (NULL)");
    REQUIRE(conn->last_insert_id() == 3);
}
