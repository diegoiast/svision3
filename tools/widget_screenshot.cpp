// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>
//
// Scratch tool for the documentation site: renders a single widget (or all
// of them) inside a window and saves a screenshot, entirely offscreen --
// no display server needed. Uses the dummy platform for windowing plus
// CairoTextRasterizer + cairo_capture() (both otherwise-internal to the
// Cairo backend) to rasterize straight into an in-memory PNG.
//
// Usage: widget_screenshot <widget-name|all> <output-dir>

#include "svision3/application.hpp"
#include "svision3/button.hpp"
#include "svision3/button_group.hpp"
#include "svision3/charts/area_chart.hpp"
#include "svision3/charts/bar_chart.hpp"
#include "svision3/charts/candlestick_chart.hpp"
#include "svision3/charts/histogram.hpp"
#include "svision3/charts/line_chart.hpp"
#include "svision3/charts/pie_chart.hpp"
#include "svision3/charts/scatter_plot.hpp"
#include "svision3/charts/stacked_bar_chart.hpp"
#include "svision3/checkbox.hpp"
#include "svision3/combobox.hpp"
#include "svision3/command.hpp"
#include "svision3/context_menu.hpp"
#include "svision3/dock_area.hpp"
#include "svision3/file_browser_widget.hpp"
#include "svision3/html_view.hpp"
#include "svision3/icon_grid.hpp"
#include "svision3/image.hpp"
#include "svision3/image_widget.hpp"
#include "svision3/item_model.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/line_input.hpp"
#include "svision3/list_view.hpp"
#include "svision3/menu.hpp"
#include "svision3/menubar.hpp"
#include "svision3/painters/cairo_painter.hpp"
#include "svision3/platform.hpp"
#include "svision3/platform/dummy_platform.hpp"
#include "svision3/progress_bar.hpp"
#include "svision3/radio_button.hpp"
#include "svision3/rich_label.hpp"
#include "svision3/scroll_area.hpp"
#include "svision3/scrollbar.hpp"
#include "svision3/slider.hpp"
#include "svision3/spin_box.hpp"
#include "svision3/splitter.hpp"
#include "svision3/status_bar.hpp"
#include "svision3/table_view.hpp"
#include "svision3/tab_widget.hpp"
#include "svision3/text_edit.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/toast_widget.hpp"
#include "svision3/toolbar.hpp"
#include "svision3/tree_view.hpp"
#include "svision3/window.hpp"
#include "svision3/xdg_image_loader.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace svision3;

namespace {

// Wraps a widget with breathing room so it doesn't render flush against the
// window edge, matching how it would normally sit inside an application.
std::shared_ptr<Widget> pad(std::shared_ptr<Widget> content) {
    auto layout = std::make_shared<VBoxLayout>();
    layout->set_margins({16, 16, 16, 16});
    layout->add_widget(std::move(content), 1, Alignment::Fill);
    return layout;
}

// Small, self-contained 16x16 XPM icons for the icon_grid demo below. Real
// icon themes (XDG lookup) aren't available in this headless tool -- no
// theme is loaded, so file_browser_widget's real filesystem icons come up
// empty too -- so icon_grid gets a handful of hand-drawn geometric icons
// via svision3::parse_xpm() instead, which needs no icon theme at all.
constexpr std::string_view kIconSquareXpm = R"(/* XPM */
static char *icon[] = {
"16 16 2 1",
"  c None",
". c #F5A623",
"                ",
"                ",
"                ",
"                ",
"    ........    ",
"    ........    ",
"    ........    ",
"    ........    ",
"    ........    ",
"    ........    ",
"    ........    ",
"    ........    ",
"                ",
"                ",
"                ",
"                "
};)";

constexpr std::string_view kIconCircleXpm = R"(/* XPM */
static char *icon[] = {
"16 16 2 1",
"  c None",
". c #4A90D9",
"                ",
"                ",
"                ",
"     ......     ",
"    ........    ",
"   ..........   ",
"   ..........   ",
"   ..........   ",
"   ..........   ",
"   ..........   ",
"   ..........   ",
"    ........    ",
"     ......     ",
"                ",
"                ",
"                "
};)";

constexpr std::string_view kIconPlusXpm = R"(/* XPM */
static char *icon[] = {
"16 16 2 1",
"  c None",
". c #9B59B6",
"                ",
"                ",
"      ....      ",
"      ....      ",
"      ....      ",
"      ....      ",
"  ............  ",
"  ............  ",
"  ............  ",
"  ............  ",
"      ....      ",
"      ....      ",
"      ....      ",
"      ....      ",
"                ",
"                "
};)";

constexpr std::string_view kIconTriangleXpm = R"(/* XPM */
static char *icon[] = {
"16 16 2 1",
"  c None",
". c #E74C3C",
"                ",
"                ",
"       ..       ",
"      ....      ",
"     ......     ",
"    ........    ",
"   ..........   ",
"  ............  ",
" .............. ",
" .............. ",
" .............. ",
" .............. ",
" .............. ",
" .............. ",
"                ",
"                "
};)";

constexpr std::string_view kIconDiamondXpm = R"(/* XPM */
static char *icon[] = {
"16 16 2 1",
"  c None",
". c #2ECC71",
"                ",
"       .        ",
"      ...       ",
"     .....      ",
"    .......     ",
"   .........    ",
"  ...........   ",
" .............  ",
"  ...........   ",
"   .........    ",
"    .......     ",
"     .....      ",
"      ...       ",
"       .        ",
"                ",
"                "
};)";

constexpr std::string_view kIconRingXpm = R"(/* XPM */
static char *icon[] = {
"16 16 2 1",
"  c None",
". c #17A2B8",
"                ",
"                ",
"                ",
"     ......     ",
"    ........    ",
"   ..      ..   ",
"   ..      ..   ",
"   ..      ..   ",
"   ..      ..   ",
"   ..      ..   ",
"   ..      ..   ",
"    ........    ",
"     ......     ",
"                ",
"                ",
"                "
};)";

// A minimal ItemModel that just pairs a label with a pre-built icon --
// icon_at() ignores the requested size and hands back the fixed 16x16
// bitmap; IconGrid's own scale_icons(true) handles scaling it up.
class IconItemModel : public ItemModel {
  public:
    explicit IconItemModel(std::vector<std::pair<std::string, Icon>> items)
        : items_(std::move(items)) {}

    size_t row_count() const override { return items_.size(); }
    std::string cell_text(size_t row, size_t) const override { return items_[row].first; }
    Icon icon_at(size_t row, size_t, int) const override { return items_[row].second; }

  private:
    std::vector<std::pair<std::string, Icon>> items_;
};

struct WidgetSpec {
    Size canvas;
    std::function<void(Window &)> setup;
};

WidgetSpec simple(Size canvas, std::function<std::shared_ptr<Widget>()> build) {
    return WidgetSpec{canvas, [build](Window &w) { w.set_root(pad(build())); }};
}

// Like simple(), but renders the widget at its natural (unstretched) width
// instead of filling the canvas -- for compact, field-like widgets where a
// full-width render looks like a stretched pill rather than the widget as
// it's normally used.
WidgetSpec natural(Size canvas, std::function<std::shared_ptr<Widget>()> build) {
    return WidgetSpec{canvas, [build](Window &w) {
                          auto layout = std::make_shared<VBoxLayout>();
                          layout->set_margins({16, 16, 16, 16});
                          layout->add_widget(build(), 0, Alignment::Start);
                          w.set_root(layout);
                      }};
}

std::map<std::string, WidgetSpec> build_registry() {
    std::map<std::string, WidgetSpec> reg;

    // ── Widgets ─────────────────────────────────────────────────────────
    reg["button"] = natural({260, 120}, [] {
        auto b = std::make_shared<Button>("&Click Me");
        return b;
    });
    reg["button_group"] = simple({340, 140}, [] {
        auto g = std::make_shared<ButtonGroup>();
        g->add_button("Left");
        g->add_button("Center");
        g->add_button("Right");
        g->set_checked(1);
        return g;
    });
    reg["label"] = simple({340, 140}, [] {
        auto l = std::make_shared<Label>("The quick brown fox jumps over the lazy dog.");
        l->set_shrinkable(true);
        return l;
    });
    reg["checkbox"] = simple({320, 140}, [] {
        auto c = std::make_shared<Checkbox>("Enable feature");
        c->set_checked(true);
        return c;
    });
    reg["radio_button"] = simple({320, 160}, [] {
        static RadioGroup group;
        auto layout = std::make_shared<VBoxLayout>();
        layout->set_spacing(8);
        layout->add_widget(std::make_shared<RadioButton>("Option 1", group));
        auto second = std::make_shared<RadioButton>("Option 2", group);
        auto *second_ptr = second.get();
        layout->add_widget(second);
        group.select(second_ptr);
        return layout;
    });
    // Shown with the dropdown open: Combobox::open_dropdown() is private, so
    // this simulates the same mouse press a real click would send -- open()
    // itself registers a Window popup, which needs the widget's real
    // (post-layout) rect to position correctly, hence the custom setup
    // instead of natural()/simple().
    reg["combobox"] = WidgetSpec{{260, 220}, [](Window &window) {
                                     auto c = std::make_shared<Combobox>(
                                         std::vector<std::string>{"Small", "Medium", "Large"});
                                     c->set_selected(1);
                                     auto layout = std::make_shared<VBoxLayout>();
                                     layout->set_margins({16, 16, 16, 16});
                                     layout->add_widget(c, 0, Alignment::Start);
                                     window.set_root(layout);
                                     c->handle_mouse(
                                         MouseEvent{MouseEvent::Type::Press, {10, 10}});
                                 }};
    // Three inputs stacked to show the common states at a glance: empty
    // with just a placeholder hint, filled with a value, and password mode.
    reg["line_input"] = WidgetSpec{{260, 190}, [](Window &window) {
                                       auto layout = std::make_shared<VBoxLayout>();
                                       layout->set_margins({16, 16, 16, 16});
                                       layout->set_spacing(10);

                                       auto empty = std::make_shared<LineInput>("Search…");
                                       layout->add_widget(empty, 0, Alignment::Start);

                                       auto filled = std::make_shared<LineInput>("Search…");
                                       filled->set_text("svision3");
                                       layout->add_widget(filled, 0, Alignment::Start);

                                       auto password = std::make_shared<LineInput>("Password");
                                       password->set_text("hunter2");
                                       password->set_password_mode(true);
                                       layout->add_widget(password, 0, Alignment::Start);

                                       window.set_root(layout);
                                   }};
    reg["text_edit"] = simple({420, 280}, [] {
        auto te = std::make_shared<TextEdit>(
            "#include <iostream>\n\nint main() {\n    std::cout << \"Hello\\n\";\n}\n");
        te->set_highlight_current_line(true);
        return te;
    });
    // SpinBox anchors its up/down buttons to its own rect's right edge, so
    // stretching it to fill the canvas (like the shared pad() helper does)
    // leaves a visual gap between the field and the buttons -- give it its
    // natural width instead.
    // SpinBox anchors its up/down buttons to its own rect's right edge, so
    // stretching it to fill the canvas leaves a visual gap -- natural width
    // keeps the buttons flush against the field.
    reg["spin_box"] = natural({220, 140}, [] { return std::make_shared<SpinBox>(42, 0, 100, 1); });
    reg["slider"] = simple({320, 120}, [] {
        auto s = std::make_shared<Slider>();
        s->set_range(0, 100);
        s->set_value(65);
        return s;
    });
    // ProgressBar's size_hint() is intentionally {0, height} -- it's meant to
    // fill whatever width it's given. Alignment::Start alone would collapse
    // it to zero width, so cap it with set_max_size() instead to keep it
    // compact while still using the fill-based layout path.
    reg["progress_bar"] = simple({220, 70}, [] {
        auto p = std::make_shared<ProgressBar>();
        p->set_value(0.62f);
        p->set_max_size({160, 100});
        return p;
    });
    reg["icon_grid"] = simple({420, 300}, [] {
        auto icon = [](std::string_view xpm) { return parse_xpm(xpm, PixelFormat::BGRA); };
        std::vector<std::pair<std::string, Icon>> items = {
            {"Documents", icon(kIconSquareXpm)}, {"Photos", icon(kIconCircleXpm)},
            {"Music", icon(kIconPlusXpm)},       {"Videos", icon(kIconTriangleXpm)},
            {"Downloads", icon(kIconDiamondXpm)}, {"Projects", icon(kIconRingXpm)},
        };
        auto model = std::make_shared<IconItemModel>(std::move(items));
        auto grid = std::make_shared<IconGrid>(model);
        grid->set_icon_size(48);
        grid->set_scale_icons(true);
        grid->set_selected(size_t(1));
        return grid;
    });
    reg["html_view"] = simple({420, 280}, [] {
        auto hv = std::make_shared<HtmlView>();
        hv->set_markdown("# SVision3\n\nA **C++20** GUI toolkit with _Cairo_ rendering.\n\n"
                         "- Widgets\n- Layouts\n- Charts");
        return hv;
    });
    reg["rich_label"] = simple({340, 140}, [] {
        auto rl = std::make_shared<RichLabel>();
        rl->set_markdown("Built with **SVision3** — see [docs](https://example.com).");
        return rl;
    });
    // fit_to_widget() needs the widget's real, final rect to compute a
    // correct zoom -- calling it before the widget is laid out (as the
    // shared simple()/natural() builders do, since they call fit_to_widget()
    // inside the widget-construction lambda, before set_root()) fits against
    // a stale/zero rect. Set the image up first, then fit only after the
    // window has actually laid the widget out.
    reg["image_widget"] = WidgetSpec{{420, 300}, [](Window &window) {
                                          auto iw = std::make_shared<ImageWidget>();
                                          iw->load("vampire-riding-a-dinozaur.png");
                                          window.set_root(pad(iw));
                                          iw->fit_to_widget();
                                      }};
    // Two toasts stacked, like they'd pile up in a real window: a plain one
    // and a rich-text one with a custom background, to show both at once.
    reg["toast_widget"] = WidgetSpec{{360, 280}, [](Window &window) {
                                          auto layout = std::make_shared<VBoxLayout>();
                                          layout->set_margins({16, 16, 16, 16});
                                          layout->set_spacing(12);

                                          auto toast1 = ToastBuilder{}
                                                            .title("Saved")
                                                            .text("Your changes have been saved.")
                                                            .timeout(4.0f)
                                                            .build();
                                          layout->add_widget(
                                              std::shared_ptr<Widget>(std::move(toast1)), 0,
                                              Alignment::Fill);

                                          auto toast2 = ToastBuilder{}
                                                            .title("Update available")
                                                            .rich_text("**v2.0** is ready to install.")
                                                            .background(Color::rgb(0.85f, 0.93f, 1.0f))
                                                            .timeout(8.0f)
                                                            .build();
                                          layout->add_widget(
                                              std::shared_ptr<Widget>(std::move(toast2)), 0,
                                              Alignment::Fill);

                                          window.set_root(layout);
                                      }};

    // ── Containers & navigation ────────────────────────────────────────
    // A 2x2 grid of the four compass orientations (skipping WestVertical/
    // EastVertical, which just rotate West/East's tab-label text) so the
    // difference is visible at a glance.
    reg["tab_widget"] = WidgetSpec{{900, 1040}, [](Window &window) {
                                       // Cycles through a fixed palette so every tab's content
                                       // pane -- across every orientation in this screenshot --
                                       // gets a visibly different color, making it obvious which
                                       // content belongs to which tab.
                                       static std::vector<Color> const content_colors = {
                                           Color::rgb(0.90f, 0.55f, 0.55f),
                                           Color::rgb(0.55f, 0.75f, 0.90f),
                                           Color::rgb(0.60f, 0.85f, 0.60f),
                                           Color::rgb(0.95f, 0.85f, 0.55f),
                                           Color::rgb(0.80f, 0.65f, 0.90f),
                                           Color::rgb(0.90f, 0.70f, 0.55f),
                                       };
                                       int color_index = 0;
                                       auto make_content = [&](std::string text) {
                                           auto label = std::make_shared<Label>(std::move(text));
                                           label->set_background_color(
                                               content_colors[color_index++ % content_colors.size()]);
                                           return label;
                                       };
                                       // orientation_name is baked into each tab's own content
                                       // label (e.g. "North Content") rather than a separate
                                       // caption widget above the TabWidget, so the cell is
                                       // self-describing from its content alone.
                                       //
                                       // Only the horizontal-text orientations (WestVertical /
                                       // EastVertical) can afford long tab titles -- North/South/
                                       // West/East get short "Tab N" titles, since on West/East
                                       // that title is rotated and its *length* becomes the tab
                                       // strip's length.
                                       auto make_tabs = [&](TabOrientation o,
                                                            std::string const &orientation_name,
                                                            int tab_count, bool short_titles) {
                                           auto tabs = std::make_shared<TabWidget>();
                                           tabs->set_orientation(o);
                                           // Closable tabs eat significant width/height in the
                                           // West/East (vertical-label) orientations at this
                                           // demo's small cell size, so keep it simple.
                                           tabs->set_tabs_closable(false);
                                           for (int i = 1; i <= tab_count; ++i) {
                                               auto title = short_titles ? "Tab " + std::to_string(i)
                                                                        : "Tab number " + std::to_string(i);
                                               tabs->add_tab(title,
                                                            make_content(orientation_name + " " + title));
                                           }
                                           tabs->set_current(0);
                                           return tabs;
                                       };
                                       auto padded = [](std::shared_ptr<Widget> tabs) {
                                           auto box = std::make_shared<VBoxLayout>();
                                           box->set_margins({4, 4, 4, 4});
                                           box->add_widget(std::move(tabs), 1, Alignment::Fill);
                                           return box;
                                       };

                                       // A 3-row stack of horizontal Splitters, two orientations
                                       // per row -- this also demonstrates Splitter itself. Each
                                       // Splitter here only ever holds 2 children: cramming all 6
                                       // into one splitter (or 3 into one column, as an earlier
                                       // version of this did) squeezes the vertical-text
                                       // orientations' tab strips far too short to render legibly.
                                       auto row = [](std::shared_ptr<Widget> a, std::shared_ptr<Widget> b) {
                                           auto s = std::make_shared<Splitter>(Orientation::Horizontal);
                                           s->add_child(std::move(a));
                                           s->add_child(std::move(b));
                                           return s;
                                       };
                                       auto rows = std::make_shared<Splitter>(Orientation::Vertical);
                                       rows->add_child(row(
                                           padded(make_tabs(TabOrientation::North, "North", 4,
                                                            /*short_titles=*/true)),
                                           padded(make_tabs(TabOrientation::South, "South", 4,
                                                            /*short_titles=*/true))));
                                       rows->add_child(row(
                                           padded(make_tabs(TabOrientation::West, "West", 4,
                                                            /*short_titles=*/true)),
                                           padded(make_tabs(TabOrientation::WestVertical,
                                                            "West (horizontal text)", 4,
                                                            /*short_titles=*/false))));
                                       rows->add_child(row(
                                           padded(make_tabs(TabOrientation::EastVertical,
                                                            "East (horizontal text)", 4,
                                                            /*short_titles=*/false)),
                                           padded(make_tabs(TabOrientation::East, "East", 4,
                                                            /*short_titles=*/true))));
                                       // Splitter's "default equal-distribution ratio" for a new
                                       // divider turns out to favor earlier children's natural
                                       // size on first layout, not a true equal split -- pin both
                                       // dividers to exact thirds. With short "Tab N" titles on
                                       // North/South/West/East, every row now needs roughly the
                                       // same amount of room.
                                       rows->set_ratio(0, 1.0f / 3.0f);
                                       rows->set_ratio(1, 2.0f / 3.0f);

                                       auto outer = std::make_shared<VBoxLayout>();
                                       outer->set_margins({16, 16, 16, 16});
                                       outer->add_widget(rows, 1, Alignment::Fill);
                                       window.set_root(outer);
                                   }};
    reg["splitter"] = simple({420, 220}, [] {
        auto sp = std::make_shared<Splitter>(Orientation::Horizontal);
        sp->add_child(std::make_shared<Label>("Left pane"));
        sp->add_child(std::make_shared<Label>("Right pane"));
        sp->set_ratio(0.35f);
        return sp;
    });
    reg["scroll_area"] = simple({340, 220}, [] {
        auto content = std::make_shared<VBoxLayout>();
        content->set_spacing(4);
        for (int i = 1; i <= 12; ++i) {
            content->add_widget(std::make_shared<Label>("Item " + std::to_string(i)));
        }
        auto area = std::make_shared<ScrollArea>();
        area->set_content(content);
        return area;
    });
    reg["scrollbar"] = simple({320, 70}, [] {
        auto sb = std::make_shared<Scrollbar>(Orientation::Horizontal);
        sb->set_range(0, 100);
        sb->set_value(30);
        return sb;
    });
    reg["dock_area"] = simple({480, 320}, [] {
        auto dock = std::make_shared<DockArea>();
        dock->set_center(std::make_shared<Label>("Editor"));
        auto files = std::make_shared<VBoxLayout>();
        files->add_widget(std::make_shared<Label>("file1.cpp"));
        files->add_widget(std::make_shared<Label>("file2.cpp"));
        dock->add_dock(DockPosition::West, "Files", files);
        dock->add_dock(DockPosition::South, "Output", std::make_shared<Label>("Build succeeded."));
        return dock;
    });
    reg["status_bar"] = simple({380, 70}, [] {
        auto sb = std::make_shared<StatusBar>();
        sb->add_section("state", "Ready");
        sb->add_section("progress", "42%");
        return sb;
    });

    // ── Data & views ────────────────────────────────────────────────────
    reg["list_view"] = simple({340, 260}, [] {
        auto model = std::make_shared<StringListModel>(
            std::vector<std::string>{"Inbox", "Sent", "Drafts", "Spam", "Trash"});
        auto lv = std::make_shared<ListView>(model);
        lv->set_alternating_row_colors(true);
        lv->set_selected(size_t(1));
        return lv;
    });
    reg["tree_view"] = simple({340, 260}, [] {
        std::vector<TreeNode> roots = {
            {"src", {{"main.cpp", {}}, {"widget.cpp", {}}}, true},
            {"include", {{"widget.hpp", {}}}, false},
            {"README.md", {}, false},
        };
        auto model = std::make_shared<SimpleTreeModel>(roots);
        auto tv = std::make_shared<TreeView>(model);
        return tv;
    });
    reg["table_view"] = simple({420, 260}, [] {
        auto model = std::make_shared<StringTableModel>(
            std::vector<std::string>{"Name", "Status", "Owner"},
            std::vector<std::vector<std::string>>{
                {"Build", "Passing", "ci"}, {"Deploy", "Pending", "ops"}, {"Tests", "Passing", "ci"}});
        auto table = std::make_shared<TableView>(model);
        table->set_alternating_row_colors(true);
        table->auto_fit_columns();
        return table;
    });
    reg["file_browser_widget"] = simple({620, 380}, [] {
        auto browser = std::make_shared<FileBrowserWidget>();
        browser->set_browser_mode(true);
        // Icon theme lookup isn't available in this headless tool (no XDG
        // icon theme installed), so icon/list mode renders with missing
        // icons -- Details mode doesn't rely on them.
        browser->set_view_mode(FileBrowserWidget::ViewMode::Details);
        browser->navigate_to(".");
        return browser;
    });

    // ── Menus, toolbars & commands ─────────────────────────────────────
    reg["menubar"] = simple({420, 100}, [] {
        auto mb = std::make_shared<MenuBar>();
        auto file_menu = mb->add_menu("File");
        file_menu->add_action("New", [] {});
        file_menu->add_action("Open…", [] {});
        auto edit_menu = mb->add_menu("Edit");
        edit_menu->add_action("Copy", [] {});
        edit_menu->add_action("Paste", [] {});
        return mb;
    });
    reg["menu"] = WidgetSpec{{320, 260}, [](Window &window) {
                                 auto mb = std::make_shared<MenuBar>();
                                 auto file_menu = mb->add_menu("File");
                                 file_menu->add_action("New", [] {});
                                 file_menu->add_action("Open…", [] {});
                                 file_menu->add_separator();
                                 file_menu->add_action("Exit", [] {});
                                 window.set_root(pad(mb));
                                 file_menu->show(&window, {24, 44});
                             }};
    reg["context_menu"] = WidgetSpec{{320, 260}, [](Window &window) {
                                          window.set_root(pad(std::make_shared<Label>(
                                              "Right-click for options")));
                                          static ContextMenu menu({
                                              MenuItem::action("Cut", [] {}),
                                              MenuItem::action("Copy", [] {}),
                                              MenuItem::action("Paste", [] {}),
                                              MenuItem::sep(),
                                              MenuItem::action("Select All", [] {}),
                                          });
                                          menu.show(&window, {60, 60});
                                      }};
    reg["toolbar"] = simple({380, 90}, [] {
        auto tb = std::make_shared<Toolbar>();
        tb->add_command(Command::create("New", [] {}));
        tb->add_command(Command::create("Open", [] {}));
        tb->add_separator();
        tb->add_command(Command::create("Save", [] {}));
        return tb;
    });

    // ── Layouts ─────────────────────────────────────────────────────────
    reg["layout"] = simple({400, 160}, [] {
        auto layout = std::make_shared<VBoxLayout>();
        layout->set_spacing(8);
        layout->add_widget(std::make_shared<Label>("Layouts stack and arrange widgets"));
        auto row = std::make_shared<HBoxLayout>();
        row->add_widget(std::make_shared<Button>("OK"));
        row->add_widget(std::make_shared<Button>("Cancel"));
        layout->add_widget(row);
        return layout;
    });

    // ── Charts ──────────────────────────────────────────────────────────
    reg["line_chart"] = simple({480, 320}, [] {
        auto chart = std::make_shared<LineChart>();
        chart->set_title("Requests/s");
        chart->add_series({"API", Color::rgb(0.2f, 0.5f, 0.9f),
                           {{0, 10, ""}, {1, 14, ""}, {2, 9, ""}, {3, 18, ""}, {4, 12, ""}}});
        return chart;
    });
    // Note: the chart's own internal right margin is tight around the last
    // category label ("Q4" clips to "Q") -- this is fixed inside the chart's
    // paint code and unaffected by outer padding/canvas width, so it's left
    // as a known minor cosmetic quirk rather than worked around here.
    reg["bar_chart"] = simple({480, 320}, [] {
        auto chart = std::make_shared<BarChart>();
        chart->set_title("Quarterly Sales");
        auto bar_color = Color::rgb(0.3f, 0.6f, 0.3f);
        chart->add_series({"2025",
                           bar_color,
                           {{0, 42, "Q1", "", bar_color},
                            {1, 55, "Q2", "", bar_color},
                            {2, 48, "Q3", "", bar_color},
                            {3, 61, "Q4", "", bar_color}}});
        return chart;
    });
    reg["stacked_bar_chart"] = simple({480, 320}, [] {
        auto chart = std::make_shared<StackedBarChart>();
        chart->set_title("I/O Ops");
        chart->set_categories({{0, "Mon"}, {1, "Tue"}, {2, "Wed"}, {3, "Thu"}});
        chart->add_series({"Reads", Color::rgb(0.2f, 0.5f, 0.8f), {10, 12, 9, 14}});
        chart->add_series({"Writes", Color::rgb(0.8f, 0.4f, 0.2f), {4, 6, 5, 7}});
        return chart;
    });
    reg["area_chart"] = simple({480, 320}, [] {
        auto chart = std::make_shared<AreaChart>();
        chart->set_title("CPU Usage");
        chart->set_y_unit_suffix("%");
        chart->add_series({"CPU", Color::rgb(0.9f, 0.3f, 0.2f), Color::rgba(0.9f, 0.3f, 0.2f, 0.3f),
                           {{0, 20, ""}, {1, 45, ""}, {2, 38, ""}, {3, 60, ""}}});
        return chart;
    });
    // Note: the legend row clips slightly against the bottom edge regardless
    // of canvas size -- PieChart doesn't appear to scale its internal layout
    // with the widget's allocated rect, so this is a known minor quirk.
    reg["pie_chart"] = simple({420, 320}, [] {
        auto chart = std::make_shared<PieChart>();
        chart->set_title("Browser Share");
        chart->set_slices({{"Chrome", 64, Color::rgb(0.9f, 0.6f, 0.1f)},
                           {"Firefox", 12, Color::rgb(0.9f, 0.4f, 0.1f)},
                           {"Other", 24, Color::rgb(0.5f, 0.5f, 0.5f)}});
        return chart;
    });
    reg["scatter_plot"] = simple({480, 320}, [] {
        auto chart = std::make_shared<ScatterPlot>();
        chart->set_title("Samples");
        chart->add_series({"Set A",
                           Color::rgb(0.2f, 0.6f, 0.9f),
                           4.0f,
                           {{1.2f, 3.4f, ""}, {2.1f, 1.9f, ""}, {3.0f, 4.2f, ""}, {1.8f, 2.6f, ""}}});
        return chart;
    });
    reg["histogram"] = simple({480, 320}, [] {
        auto chart = std::make_shared<Histogram>();
        chart->set_title("Latency Distribution");
        chart->set_bin_count(12);
        std::vector<float> samples;
        for (int i = 0; i < 200; ++i) {
            samples.push_back(20.0f + 10.0f * std::sin(i * 0.3f) + static_cast<float>(i % 7));
        }
        chart->add_series({"Latency (ms)", Color::rgb(0.3f, 0.5f, 0.8f), samples});
        return chart;
    });
    // Note: same known quirk as bar_chart above -- the last category label
    // ("Thu") clips slightly against the chart's own fixed internal margin.
    reg["candlestick_chart"] = simple({480, 320}, [] {
        auto chart = std::make_shared<CandlestickChart>();
        chart->set_title("AAPL");
        CandlestickSeries series{
            .name = "AAPL",
            .candles =
                {
                    {0, 189.5f, 191.2f, 188.9f, 190.7f, "Mon"},
                    {1, 190.7f, 192.0f, 189.5f, 189.9f, "Tue"},
                    {2, 189.9f, 190.5f, 187.2f, 188.0f, "Wed"},
                    {3, 188.0f, 191.0f, 187.5f, 190.8f, "Thu"},
                },
        };
        chart->add_series(series);
        return chart;
    });

    return reg;
}

std::map<std::string, WidgetSpec> &registry() {
    static std::map<std::string, WidgetSpec> reg = build_registry();
    return reg;
}

struct ThemeEntry {
    ThemeStyle style;
    char const *slug;
};

// The concrete, selectable theme styles -- ThemeStyle::System is "follow the
// OS's own convention" rather than a distinct look, so it's skipped here.
std::vector<ThemeEntry> const &supported_themes() {
    static std::vector<ThemeEntry> themes = {
        {ThemeStyle::MacOS, "macos"},   {ThemeStyle::Material, "material"},
        {ThemeStyle::Win11, "win11"},   {ThemeStyle::Win95, "win95"},
        {ThemeStyle::Plasma6, "plasma6"}, {ThemeStyle::GNOME, "gnome"},
    };
    return themes;
}

bool render_one(Application &app, PlatformApplication &platform, std::string const &key,
                WidgetSpec const &spec, std::filesystem::path const &out_dir) {
    auto window = app.create_window(key, spec.canvas);
    spec.setup(*window);
    auto icon = cairo_capture(window.get());
    if (!icon) {
        std::cerr << "capture failed for " << key << "\n";
        return false;
    }
    auto path = (out_dir / (key + ".png")).string();
    if (!platform.get_image_loader()->save(*icon, path)) {
        std::cerr << "save failed for " << key << "\n";
        return false;
    }
    std::cout << "wrote " << path << "\n";
    return true;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "usage: widget_screenshot <widget-name|all> <output-dir>\n";
        return 1;
    }
    std::string name = argv[1];
    std::filesystem::path out_dir = argv[2];
    std::filesystem::create_directories(out_dir);

    setenv("SVISION_BACKEND", "dummy", 1);
    Application app;
    auto *platform = detail::current_platform();
    platform->set_rasterizer(new CairoTextRasterizer());
    // Widgets like FileBrowserWidget draw real XDG icons (go-previous,
    // folder-new, ...) rather than text glyphs, so without a theme those
    // buttons render empty. Faenza is bundled in the repo -- set_theme()
    // resolves "themes/<name>" relative to the current working directory,
    // so this only works run from the repo root.
    auto icon_loader = std::make_unique<XdgImageLoader>("Faenza");
    if (!icon_loader->theme_loaded()) {
        std::cerr << "warning: Faenza icon theme not found -- run this from the "
                    "repository root\n";
    }
    app.set_icon_provider(std::move(icon_loader));

    auto &reg = registry();
    WidgetSpec const *single = nullptr;
    if (name != "all") {
        auto it = reg.find(name);
        if (it == reg.end()) {
            std::cerr << "unknown widget: " << name << "\n";
            return 1;
        }
        single = &it->second;
    }

    // One PNG per (widget, theme) combination, organized as
    // <output-dir>/<theme-slug>/<widget>.png -- every theme svision3
    // ships gets rendered automatically, no per-theme invocation needed.
    bool ok = true;
    for (auto const &theme : supported_themes()) {
        Theme::set_current(ThemeFactory::create(theme.style));
        auto theme_dir = out_dir / theme.slug;
        std::filesystem::create_directories(theme_dir);

        if (single) {
            ok = render_one(app, *platform, name, *single, theme_dir) && ok;
            continue;
        }
        for (auto const &[key, spec] : reg) {
            ok = render_one(app, *platform, key, spec, theme_dir) && ok;
        }
    }
    return ok ? 0 : 1;
}
