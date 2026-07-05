#include "toolkit/application.hpp"
#include "toolkit/button.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/table_view.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include "db/connection.hpp"
#include "toolkit/file_dialog.hpp"

#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include <vector>

static std::unique_ptr<db::Connection> g_db;
static std::vector<std::string> g_known_schemas;

static std::string quote_id(std::string const &id) {
    std::string q = "\"";
    for (char c : id) {
        if (c == '"') q += "\"\"";
        else q += c;
    }
    q += '"';
    return q;
}

static std::vector<std::string> get_schemas() {
    g_known_schemas.clear();
    if (!g_db) return {};
    auto stmt = g_db->prepare("PRAGMA database_list");
    auto rs = stmt->execute();
    while (rs->next())
        g_known_schemas.push_back(rs->get_string(1));
    return g_known_schemas;
}

static bool is_known_schema(std::string const &schema) {
    for (auto const &s : g_known_schemas)
        if (s == schema) return true;
    return false;
}

static std::vector<std::string> get_tables(std::string const &schema) {
    if (!g_db) return {};
    if (!is_known_schema(schema)) {
        spdlog::error("unknown schema: {}", schema);
        return {};
    }
    std::string sql = "SELECT name FROM " + quote_id(schema) +
                      ".sqlite_master WHERE type IN ('table','view') ORDER BY name";
    spdlog::info("SQL: {}", sql);
    std::vector<std::string> results;
    try {
        auto stmt = g_db->prepare(sql);
        auto rs = stmt->execute();
        while (rs->next())
            results.push_back(rs->get_string(0));
    } catch (db::Error const &e) {
        spdlog::error("query error: {}", e.what());
    }
    return results;
}

static void run_query(std::string const &sql,
                      std::shared_ptr<toolkit::StringTableModel> &model) {
    if (!g_db) {
        model->set_data({"Error"}, {{"No database open"}});
        return;
    }
    spdlog::info("SQL: {}", sql);
    try {
        auto stmt = g_db->prepare(sql);
        auto rs = stmt->execute();

        int ncols = rs->column_count();
        std::vector<std::string> headers;
        headers.reserve(ncols);
        for (int c = 0; c < ncols; c++)
            headers.push_back(rs->column_name(c));

        std::vector<std::vector<std::string>> rows;
        while (rs->next()) {
            std::vector<std::string> row;
            row.reserve(ncols);
            for (int c = 0; c < ncols; c++) {
                if (rs->is_null(c))
                    row.emplace_back("NULL");
                else
                    row.push_back(rs->get_string(c));
            }
            rows.push_back(std::move(row));
        }

        model->set_data(std::move(headers), std::move(rows));
    } catch (db::Error const &e) {
        spdlog::error("SQL error: {}", e.what());
        model->set_data({"Error"}, {{e.what()}});
    }
}


struct ViewerState {
    toolkit::Window *window = nullptr;
    toolkit::Combobox *schema_combo = nullptr;
    toolkit::Combobox *table_combo = nullptr;
    toolkit::LineInput *query_input = nullptr;
    toolkit::Label *status = nullptr;
    std::shared_ptr<toolkit::StringTableModel> table_model;
};

static void open_database(ViewerState &vs, std::string const &path) {
    try {
        g_db = db::open("sqlite:" + path);
    } catch (db::Error const &e) {
        spdlog::error("Cannot open '{}': {}", path, e.what());
        vs.status->set_text("Error: " + std::string(e.what()));
        return;
    }

    vs.window->request_redraw();
    spdlog::info("Opened: {}", path);

    vs.schema_combo->set_items(get_schemas());
    vs.schema_combo->set_selected(0);

    auto schema = vs.schema_combo->selected_text();
    if (schema.empty()) schema = "main";
    vs.table_combo->set_items(get_tables(schema));
    vs.table_combo->set_selected(0);

    auto table = vs.table_combo->selected_text();
    if (!table.empty()) {
        vs.query_input->set_text("SELECT * FROM " + quote_id(table) + " LIMIT 1000");
        auto sql = vs.query_input->text();
        run_query(sql, vs.table_model);
        vs.status->set_text(std::to_string(vs.table_model->row_count()) + " row(s), " +
                            std::to_string(vs.table_model->column_count()) + " column(s)");
    } else {
        vs.query_input->set_text("");
        vs.table_model->set_data({"(no tables)"}, {});
        vs.status->set_text(path);
    }
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);

    toolkit::Application app;
    auto *window = app.create_window("SQLite Viewer", {800, 600});

    ViewerState vs;
    vs.window = window;
    vs.table_model = std::make_shared<toolkit::StringTableModel>(
        std::vector<std::string>{"(no data)"});

    auto root = std::make_unique<toolkit::VBoxLayout>();
    root->set_margins({12, 12, 12, 12});
    root->set_spacing(8);

    // Toolbar row: Open button, schema combo, table combo
    auto toolbar = std::make_unique<toolkit::HBoxLayout>();
    toolbar->set_spacing(8);

    auto open_btn = std::make_unique<toolkit::Button>("&Open...");
    open_btn->on_click = [&vs] {
        toolkit::FileDialog(vs.window)
            .add_filter("SQLite databases", "*.db *.sqlite *.sqlite3 *.db3")
            .use_native()
            .open()
            .then([&vs](toolkit::FileDialog::Result path) {
                if (path)
                    open_database(vs, *path);
            });
    };
    toolbar->add_widget(std::move(open_btn));

    auto schema_combo = std::make_unique<toolkit::Combobox>();
    vs.schema_combo = schema_combo.get();
    auto schema_label = std::make_unique<toolkit::Label>("&Schema:");
    schema_label->set_buddy(vs.schema_combo);
    toolbar->add_widget(std::move(schema_label));
    toolbar->add_widget(std::move(schema_combo));

    auto table_combo = std::make_unique<toolkit::Combobox>();
    vs.table_combo = table_combo.get();
    auto table_label = std::make_unique<toolkit::Label>("&Table:");
    table_label->set_buddy(vs.table_combo);
    toolbar->add_widget(std::move(table_label));
    toolbar->add_widget(std::move(table_combo), 1);

    root->add_widget(std::move(toolbar));

    // Query input
    auto query_input = std::make_unique<toolkit::LineInput>();
    vs.query_input = query_input.get();
    root->add_widget(std::move(query_input));

    // Results table
    auto table_view = std::make_unique<toolkit::TableView>(vs.table_model);
    table_view->set_alternating_row_colors(true);
    root->add_widget(std::move(table_view), 1);

    // Bottom bar
    auto bottom = std::make_unique<toolkit::HBoxLayout>();
    bottom->set_spacing(8);

    auto status = std::make_unique<toolkit::Label>("");
    vs.status = status.get();
    bottom->add_widget(std::move(status), 1);

    auto quit_btn = std::make_unique<toolkit::Button>("&Quit");
    quit_btn->on_click = [window] { window->close(); };
    bottom->add_widget(std::move(quit_btn));

    root->add_widget(std::move(bottom));

    // Execute query callback
    auto execute_query = [&vs]() {
        auto sql = vs.query_input->text();
        if (sql.empty()) return;
        run_query(sql, vs.table_model);
        vs.status->set_text(std::to_string(vs.table_model->row_count()) + " row(s), " +
                            std::to_string(vs.table_model->column_count()) + " column(s)");
    };

    vs.query_input->on_submit = [execute_query](std::string const &, auto &) { execute_query(); };

    // Wire up combos
    auto update_tables = [&vs]() {
        auto schema = vs.schema_combo->selected_text();
        if (schema.empty()) schema = "main";
        vs.table_combo->set_items(get_tables(schema));
        if (vs.table_combo->selected() < 0)
            vs.table_combo->set_selected(0);
    };

    auto on_table_selected = [&vs, execute_query](int) {
        auto table = vs.table_combo->selected_text();
        if (table.empty()) return;
        vs.query_input->set_text("SELECT * FROM " + quote_id(table) + " LIMIT 1000");
        execute_query();
    };

    vs.schema_combo->on_change = [update_tables, on_table_selected](int) {
        update_tables();
        on_table_selected(0);
    };

    vs.table_combo->on_change = on_table_selected;

    // If a path was given on command line, open it immediately
    if (argc >= 2)
        open_database(vs, argv[1]);

    window->set_root(std::move(root));
    window->show();

    return app.run();
}
