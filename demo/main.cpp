#include "toolkit/application.hpp"
#include "toolkit/button.hpp"
#include "toolkit/checkbox.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/list_view.hpp"
#include "toolkit/progress_bar.hpp"
#include "toolkit/radio_button.hpp"
#include "toolkit/spin_box.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/table_view.hpp"
#include "toolkit/text_edit.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <fstream>
#include <nfd.h>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

static toolkit::RadioGroup scheme_group;
static toolkit::ThemeStyle current_style = toolkit::ThemeStyle::MacOS; // Placeholder, set in main
static toolkit::ColorScheme current_scheme = toolkit::ColorScheme::Light;

auto beatlesSongs = std::vector<std::string>{
    "A Day in the Life",
    "A Hard Day's Night",
    "A Taste of Honey",
    "Across the Universe",
    "Act Naturally",
    "All I've Got to Do",
    "All My Loving",
    "All Together Now",
    "All You Need Is Love",
    "And I Love Her",
    "And Your Bird Can Sing",
    "Anna (Go to Him)",
    "Another Girl",
    "Any Time at All",
    "Ask Me Why",
    "Baby It's You",
    "Baby You Can Drive My Car",
    "Baby's in Black",
    "Back in the U.S.S.R.",
    "Bad Boy",
    "Because",
    "Being for the Benefit of Mr. Kite!",
    "Birthday",
    "Blackbird",
    "Blue Jay Way",
    "Boys",
    "Can't Buy Me Love",
    "Carry That Weight",
    "Chains",
    "Come Together",
    "Continuing Story of Bungalow Bill",
    "Cry Baby Cry",
    "Day Tripper",
    "Dear Prudence",
    "Devil in Her Heart",
    "Dig a Pony",
    "Dig It",
    "Dizzy Miss Lizzy",
    "Do You Want to Know a Secret",
    "Doctor Robert",
    "Don't Bother Me",
    "Don't Let Me Down",
    "Don't Pass Me By",
    "Drive My Car",
    "Eight Days a Week",
    "Eleanor Rigby",
    "Every Little Thing",
    "Everybody's Got Something to Hide Except Me and My Monkey",
    "Everybody's Trying to Be My Baby",
    "Fixing a Hole",
    "Flying",
    "For No One",
    "For You Blue",
    "From Me to You",
    "Get Back",
    "Getting Better",
    "Girl",
    "Glass Onion",
    "Golden Slumbers",
    "Good Day Sunshine",
    "Good Morning Good Morning",
    "Good Night",
    "Got to Get You into My Life",
    "Happiness Is a Warm Gun",
    "Hello, Goodbye",
    "Help!",
    "Helter Skelter",
    "Her Majesty",
    "Here Comes The Sun",
    "Here, There and Everywhere",
    "Hey Bulldog",
    "Hey Jude",
    "Hold Me Tight",
    "Honey Don't",
    "Honey Pie",
    "I Am The Walrus",
    "I Call Your Name",
    "I Don't Want to Spoil the Party",
    "I Feel Fine",
    "I Me Mine",
    "I Need You",
    "I Saw Her Standing There",
    "I Should Have Known Better",
    "I Wanna Be Your Man",
    "I Want to Hold Your Hand",
    "I Want to Tell You",
    "I Want You (She's So Heavy)",
    "I Will",
    "I'll Be Back",
    "I'll Cry Instead",
    "I'll Follow the Sun",
    "I'll Get You",
    "I'm a Loser",
    "I'm Down",
    "I'm Happy Just to Dance with You",
    "I'm Looking Through You",
    "I'm Only Sleeping",
    "I'm So Tired",
    "I've Got a Feeling",
    "I've Just Seen a Face",
    "If I Fell",
    "If I Needed Someone",
    "In My Life",
    "In Spite of All the Danger",
    "It Won't Be Long",
    "It's All Too Much",
    "It's Only Love",
    "Julia",
    "Kansas City/Hey-Hey-Hey-Hey!",
    "Lady Madonna",
    "Let It Be",
    "Little Child",
    "Long Tall Sally",
    "Long, Long, Long",
    "Love Me Do",
    "Love You To",
    "Lovely Rita",
    "Lucy In The Sky With Diamonds",
    "Maggie Mae",
    "Magic Mystery Tour",
    "Martha My Dear",
    "Matchbox",
    "Maxwell's Silver Hammer",
    "Mean Mr. Mustard",
    "Michelle",
    "Misery",
    "Money (That's What I Want)",
    "Mother Nature's Son",
    "Mr. Moonlight",
    "No Reply",
    "Norwegian Wood",
    "Not a Second Time",
    "Nowhere Man",
    "Ob-La-Di, Ob-La-Da",
    "Octopus's Garden",
    "Oh! Darling",
    "Old Brown Shoe",
    "One After 909",
    "Only a Northern Song",
    "P.S. I Love You",
    "Paperback Writer",
    "Penny Lane",
    "Piggies",
    "Please Mister Postman",
    "Please Please Me",
    "Polythene Pam",
    "Rain",
    "Revolution",
    "Revolution 1",
    "Revolution 9",
    "Rock and Roll Music",
    "Rocky Raccoon",
    "Roll Over Beethoven",
    "Run for Your Life",
    "Savoy Truffle",
    "Sexy Sadie",
    "Sgt. Pepper's Lonely Hearts Club Band",
    "Sgt. Pepper's Lonely Hearts Club Band (Reprise)",
    "She Came in Through the Bathroom Window",
    "She Loves You",
    "She Said She Said",
    "She's a Woman",
    "She's Leaving Home",
    "Slow Down",
    "Something",
    "Strawberry Fields Forever",
    "Sun King",
    "Taxman",
    "Tell Me What You See",
    "Tell Me Why",
    "Thank You Girl",
    "The Ballad of John and Yoko",
    "The End",
    "The Fool on the Hill",
    "The Inner Light",
    "The Long and Winding Road",
    "The Night Before",
    "The Word",
    "There's a Place",
    "Things We Said Today",
    "Think for Yourself",
    "This Boy",
    "Ticket To Ride",
    "Till There Was You",
    "Tomorrow Never Knows",
    "Twist And Shout",
    "Two of Us",
    "Wait",
    "We Can Work It Out",
    "What Goes On",
    "What You're Doing",
    "When I Get Home",
    "When I'm Sixty-Four",
    "While My Guitar Gently Weeps",
    "Why Don't We Do It in the Road?",
    "Wild Honey Pie",
    "With A Little Help From My Friends",
    "Within You Without You",
    "Words of Love",
    "Yellow Submarine",
    "Yer Blues",
    "Yes It Is",
    "YesterdaybeatlesSongsLength",
    "You Can't Do That",
    "You Know My Name (Look Up the Number)",
    "You Like Me Too Much",
    "You Never Give Me Your Money",
    "You Really Got a Hold on Me",
    "You Won't See Me",
    "You're Going to Lose That Girl",
    "You've Got to Hide Your Love Away",
    "Your Mother Should Know",
};

auto beatlesSongsLength = std::vector<std::vector<std::string>>{
    {"Come Together", "Abbey Road", "1969", "4:19"},
    {"Something", "Abbey Road", "1969", "3:02"},
    {"Here Comes The Sun", "Abbey Road", "1969", "3:05"},
    {"Let It Be", "Let It Be", "1970", "4:03"},
    {"Hey Jude", "Single", "1968", "7:11"},
    {"Yesterday", "Help!", "1965", "2:05"},
    {"A Day in the Life", "Sgt. Pepper's", "1967", "5:33"},
    {"Strawberry Fields Forever", "Single", "1967", "4:07"},
    {"Eleanor Rigby", "Revolver", "1966", "2:08"},
    {"Norwegian Wood", "Rubber Soul", "1965", "2:05"},
    {"In My Life", "Rubber Soul", "1965", "2:27"},
    {"Blackbird", "White Album", "1968", "2:18"},
    {"While My Guitar Gently Weeps", "White Album", "1968", "4:45"},
    {"Across the Universe", "Let It Be", "1970", "3:48"},
    {"Twist And Shout", "Please Please Me", "1963", "2:33"},
    {"I Want to Hold Your Hand", "Single", "1963", "2:26"},
    {"Penny Lane", "Single", "1967", "3:03"},
    {"Lucy In The Sky With Diamonds", "Sgt. Pepper's", "1967", "3:28"},
    {"Helter Skelter", "White Album", "1968", "4:30"},
    {"Golden Slumbers", "Abbey Road", "1969", "1:31"},
};

static void apply_theme(toolkit::Window *window) {
    toolkit::Theme::set_current(toolkit::Theme::create(current_style, current_scheme));
    window->request_redraw();
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);

    toolkit::Application app;

    std::string screenshot_path;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.starts_with("--screenshot=")) {
            screenshot_path = arg.substr(13);
        }
    }
    current_style = toolkit::Theme::detect_system_style();
    toolkit::Theme::set_current(toolkit::Theme::create(current_style, current_scheme));

    auto *window = app.create_window("Demo", {600, 400});

    auto root = std::make_unique<toolkit::VBoxLayout>();
    root->set_margins({0, 0, 0, 0});
    root->set_spacing(0);

    auto tabs = std::make_unique<toolkit::TabWidget>();
    auto *tabs_ptr = tabs.get();

    auto b = std::make_unique<toolkit::Button>("Menu");
    b->set_flat(true);
    b->set_background_color(toolkit::Color::rgb(0.9f, 0.75f, 0.6f));
    b->set_flat(true);
    b->set_focusable(false);
    b->set_padding({2, 8, 2, 8});
    tabs->set_leading_widget(std::move(b));

    // ── Tab: Main ──────────────────────────────────────────────────────
    auto tab_main = std::make_unique<toolkit::VBoxLayout>();
    tab_main->set_margins({20, 20, 20, 20});
    tab_main->set_spacing(12);

    auto label = std::make_unique<toolkit::Label>(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
        "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.");
    label->set_shrinkable(true);
    label->set_tooltip(label->text());
    tab_main->add_widget(std::move(label));

    auto cb1 = std::make_unique<toolkit::Checkbox>("Enable notifications");
    cb1->set_checked(true);
    cb1->set_tooltip("Toggle desktop notifications");
    tab_main->add_widget(std::move(cb1));

    std::vector<std::string> style_names;
    for (int i = 0; i < toolkit::theme_style_count; i++) {
        style_names.push_back(toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i)));
    }

    auto combo = std::make_unique<toolkit::Combobox>(style_names);
    combo->set_selected(static_cast<int>(current_style));
    combo->set_tooltip("Select a theme style");
    combo->on_change = [window](int index) {
        current_style = static_cast<toolkit::ThemeStyle>(index);
        apply_theme(window);
    };
    tab_main->add_widget(std::move(combo));

    auto rb_light = std::make_unique<toolkit::RadioButton>("Light", scheme_group);
    rb_light->set_tooltip("Light color scheme");
    auto rb_dark = std::make_unique<toolkit::RadioButton>("Dark", scheme_group);
    rb_dark->set_tooltip("Dark color scheme");
    auto rb_pink = std::make_unique<toolkit::RadioButton>("Pink", scheme_group);
    rb_pink->set_tooltip("Pink color scheme");
    scheme_group.select(rb_light.get());
    scheme_group.on_change = [window](int index) {
        constexpr toolkit::ColorScheme schemes[] = {
            toolkit::ColorScheme::Light, toolkit::ColorScheme::Dark, toolkit::ColorScheme::Pink};
        current_scheme = schemes[index];
        apply_theme(window);
    };
    tab_main->add_widget(std::move(rb_light));
    tab_main->add_widget(std::move(rb_dark));
    tab_main->add_widget(std::move(rb_pink));

    auto progress = std::make_unique<toolkit::ProgressBar>();
    auto *progress_ptr = progress.get();
    progress->set_value(0.0f);
    progress->set_tooltip("0%");
    tab_main->add_widget(std::move(progress));

    tab_main->add_widget(std::make_unique<toolkit::Label>(
        fmt::format("Platform: {} | Painter: {}", app.platform_name(), app.painter_name())));

    auto debug_toggle = std::make_unique<toolkit::Checkbox>("Show Debug Frames");
    debug_toggle->on_toggle = [window](bool checked) {
        toolkit::Widget::debug_show_frames = checked;
        window->request_redraw();
    };
    tab_main->add_widget(std::move(debug_toggle));

    tabs->add_tab("Main", std::move(tab_main));

    // ── Tab: Inputs ────────────────────────────────────────────────────
    auto tab_inputs = std::make_unique<toolkit::VBoxLayout>();
    tab_inputs->set_margins({20, 20, 20, 20});
    tab_inputs->set_spacing(12);

    auto input1 = std::make_unique<toolkit::LineInput>("Regular input");
    tab_inputs->add_widget(std::move(input1));

    auto input2 = std::make_unique<toolkit::LineInput>("Password input");
    input2->set_password_mode(true);
    tab_inputs->add_widget(std::move(input2));

    auto input3 = std::make_unique<toolkit::LineInput>("Read-only input");
    input3->set_text("This text cannot be edited");
    input3->set_read_only(true);
    tab_inputs->add_widget(std::move(input3));

    auto inputs_spacer = std::make_unique<toolkit::Label>("");
    tab_inputs->add_widget(std::move(inputs_spacer), 1);

    tabs->add_tab("Inputs", std::move(tab_inputs));

    // ── Tab 3: Songs ─────────────────────────────────────────────────────
    auto tab3 = std::make_unique<toolkit::VBoxLayout>();
    tab3->set_margins({20, 20, 20, 20});
    tab3->set_spacing(12);

    auto songs_adapter = std::make_shared<toolkit::StringListAdapter>(beatlesSongs);

    auto filter_adapter = std::make_shared<toolkit::FilterAdapter>(songs_adapter);
    filter_adapter->set_simulated_delay_ms(10);

    auto filter_row = std::make_unique<toolkit::HBoxLayout>();
    filter_row->set_spacing(8);

    auto filter_input = std::make_unique<toolkit::LineInput>("Filter songs...");
    filter_input->set_tooltip("Type to filter the song list");
    filter_input->on_change = [filter_adapter](std::string const &text) {
        filter_adapter->set_filter(text);
    };
    filter_row->add_widget(std::move(filter_input), 1);

    auto delay_label = std::make_unique<toolkit::Label>("Delay (ms):");
    filter_row->add_widget(std::move(delay_label));

    auto delay_spin = std::make_unique<toolkit::SpinBox>(10, 0, 200, 5);
    delay_spin->set_tooltip("Simulated delay per item (ms)");
    delay_spin->on_change = [filter_adapter](int val) {
        filter_adapter->set_simulated_delay_ms(val);
    };
    filter_row->add_widget(std::move(delay_spin));

    tab3->add_widget(std::move(filter_row));

    auto filter_progress = std::make_unique<toolkit::ProgressBar>();
    auto *filter_progress_ptr = filter_progress.get();
    filter_progress->set_visible(false);
    tab3->add_widget(std::move(filter_progress));

    filter_adapter->on_progress = [filter_progress_ptr, window](float progress) {
        if (progress < 0.0f) {
            filter_progress_ptr->set_visible(false);
        } else {
            filter_progress_ptr->set_visible(true);
            filter_progress_ptr->set_value(progress);
        }
        window->request_redraw();
    };

    auto songs_list = std::make_unique<toolkit::ListView>(filter_adapter);
    songs_list->set_alternating_row_colors(true);
    songs_list->set_multi_select(true);
    songs_list->on_selection_changed = [filter_adapter](int idx) {
        if (idx >= 0) {
            int src = filter_adapter->source_index(idx);
            spdlog::info("Selected: [{}] {}", src, filter_adapter->text_at(idx));
        }
    };
    tab3->add_widget(std::move(songs_list), 1);

    tabs->add_tab("Songs", std::move(tab3));

    // ── Tab 4: Table ──────────────────────────────────────────────────────
    auto tab4 = std::make_unique<toolkit::VBoxLayout>();
    tab4->set_margins({20, 20, 20, 20});
    tab4->set_spacing(12);

    auto table_model = std::make_shared<toolkit::StringTableModel>(
        std::vector<std::string>{"Song", "Album", "Year", "Duration"}, beatlesSongsLength);

    auto table = std::make_unique<toolkit::TableView>(table_model);
    table->set_alternating_row_colors(true);
    table->set_multi_select(true);
    table->set_column_width(0, 220.0f);
    table->set_column_width(1, 140.0f);
    table->set_column_width(2, 60.0f);
    table->set_column_width(3, 80.0f);
    table->on_selection_changed = [table_model](int row) {
        if (row >= 0) {
            spdlog::info("Table: {} - {}", table_model->cell_text(row, 0),
                         table_model->cell_text(row, 1));
        }
    };
    tab4->add_widget(std::move(table), 1);

    tabs->add_tab("Table", std::move(tab4));

    // ── Tab 5: Editor ─────────────────────────────────────────────────────
    auto tab5 = std::make_unique<toolkit::VBoxLayout>();
    tab5->set_margins({20, 20, 20, 20});
    tab5->set_spacing(12);

    auto editor_toolbar = std::make_unique<toolkit::HBoxLayout>();
    editor_toolbar->set_spacing(8);

    auto editor =
        std::make_unique<toolkit::TextEdit>("#include <iostream>\n"
                                            "\n"
                                            "int main() {\n"
                                            "    std::cout << \"Hello, world!\" << std::endl;\n"
                                            "    return 0;\n"
                                            "}\n");
    auto *editor_ptr = editor.get();

    auto open_btn = std::make_unique<toolkit::Button>("Open...");
    open_btn->set_tooltip("Open a text file");
    open_btn->on_click = [editor_ptr] {
        NFD_Init();
        nfdu8char_t *out_path = nullptr;
        nfdresult_t result = NFD_OpenDialogU8(&out_path, nullptr, 0, nullptr);
        if (result == NFD_OKAY && out_path) {
            std::ifstream f(out_path);
            if (f) {
                std::string contents((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
                editor_ptr->set_text(contents);
            }
            NFD_FreePathU8(out_path);
        }
        NFD_Quit();
    };
    editor_toolbar->add_widget(std::move(open_btn));

    auto toolbar_spacer = std::make_unique<toolkit::Label>("");
    editor_toolbar->add_widget(std::move(toolbar_spacer), 1);

    tab5->add_widget(std::move(editor_toolbar));
    tab5->add_widget(std::move(editor), 1);

    tabs->add_tab("Editor", std::move(tab5));

    // ── Tab 6: Tabs (Orientations) ────────────────────────────────────────
    auto tab6 = std::make_unique<toolkit::VBoxLayout>();
    tab6->set_margins({0, 0, 0, 0});
    tab6->set_spacing(0);

    auto make_plus = []() {
        auto b = std::make_unique<toolkit::Button>("+");
        b->set_flat(true);
        b->set_focusable(false);
        b->set_padding({2, 8, 2, 8});
        return b;
    };
    auto make_close = []() {
        auto b = std::make_unique<toolkit::Button>("x");
        b->set_flat(true);
        b->set_focusable(false);
        b->set_padding({2, 8, 2, 8});
        b->set_background_color(toolkit::Color::rgb(1.0f, 0.8f, 0.8f));
        return b;
    };

    auto south_tabs = std::make_unique<toolkit::TabWidget>();
    south_tabs->set_orientation(toolkit::TabOrientation::South);
    south_tabs->set_leading_widget(make_plus());
    south_tabs->set_trailing_widget(make_close());
    for (int i = 1; i <= 5; ++i) {
        auto label = std::make_unique<toolkit::Label>(fmt::format("South Tab {} Content", i));
        label->set_alignment(toolkit::Alignment::Center);
        label->set_background_color(toolkit::Color::rgb(1.0f, 1.0f - i * 0.1f, 1.0f - i * 0.1f));
        south_tabs->add_tab(fmt::format("South {}", i), std::move(label));
    }

    auto side_row = std::make_unique<toolkit::HBoxLayout>();
    side_row->set_margins({0, 0, 0, 0});
    side_row->set_spacing(0);

    auto west_tabs = std::make_unique<toolkit::TabWidget>();
    west_tabs->set_orientation(toolkit::TabOrientation::West);
    west_tabs->set_leading_widget(make_plus());
    west_tabs->set_trailing_widget(make_close());
    for (int i = 1; i <= 5; ++i) {
        auto label = std::make_unique<toolkit::Label>(fmt::format("West Tab {} Content", i));
        label->set_alignment(toolkit::Alignment::Center);
        label->set_background_color(toolkit::Color::rgb(1.0f - i * 0.1f, 1.0f, 1.0f - i * 0.1f));
        west_tabs->add_tab(fmt::format("West {}", i), std::move(label));
    }

    auto east_tabs = std::make_unique<toolkit::TabWidget>();
    east_tabs->set_orientation(toolkit::TabOrientation::East);
    east_tabs->set_leading_widget(make_plus());
    east_tabs->set_trailing_widget(make_close());
    for (int i = 1; i <= 5; ++i) {
        auto label = std::make_unique<toolkit::Label>(fmt::format("East Tab {} Content", i));
        label->set_alignment(toolkit::Alignment::Center);
        label->set_background_color(toolkit::Color::rgb(1.0f - i * 0.1f, 1.0f - i * 0.1f, 1.0f));
        east_tabs->add_tab(fmt::format("East {}", i), std::move(label));
    }

    side_row->add_widget(std::move(west_tabs), 1, toolkit::Alignment::Fill);
    side_row->add_widget(std::move(east_tabs), 1, toolkit::Alignment::Fill);

    tab6->add_widget(std::move(side_row), 1);
    tab6->add_widget(std::move(south_tabs), 0);

    tabs->add_tab("Tabs", std::move(tab6));

    root->add_widget(std::move(tabs), 1);

    // ── Button bar ───────────────────────────────────────────────────────
    auto bar_wrapper = std::make_unique<toolkit::VBoxLayout>();
    bar_wrapper->set_margins({20, 10, 20, 10});

    auto button_bar = std::make_unique<toolkit::HBoxLayout>();
    button_bar->set_spacing(10);

    auto ok_btn = std::make_unique<toolkit::Button>("&ok");
    ok_btn->on_click = [] { spdlog::info("Button clicked!"); };
    ok_btn->set_tooltip("Log a message to the console");
    button_bar->add_widget(std::move(ok_btn));

    auto disabled_btn = std::make_unique<toolkit::Button>("Disabled");
    disabled_btn->set_background_color(toolkit::Color::rgb(0.3f, 0.5f, 0.3f));
    disabled_btn->set_enabled(false);
    disabled_btn->set_tooltip("This button is disabled");
    button_bar->add_widget(std::move(disabled_btn));

    auto btn_spacer = std::make_unique<toolkit::Label>("");
    button_bar->add_widget(std::move(btn_spacer), 1);

    auto quit_btn = std::make_unique<toolkit::Button>("&Quit");
    quit_btn->on_click = [window] { window->close(); };
    quit_btn->set_tooltip("Close the application (Cmd+Q)");
    button_bar->add_widget(std::move(quit_btn));

    bar_wrapper->add_widget(std::move(button_bar));
    root->add_widget(std::move(bar_wrapper));

    window->set_root(std::move(root));

    if (!screenshot_path.empty()) {
        bool ok = window->save_to_png(screenshot_path);
        spdlog::info("Screenshot saved to '{}': {}", screenshot_path, ok ? "success" : "failed");
        return ok ? 0 : 1;
    }

    window->show();

    window->start_timer(0.25f, [progress_ptr] {
        float v = progress_ptr->value() + 0.01f;
        if (v > 1.0f) {
            v = 0.0f;
        }
        progress_ptr->set_value(v);
        progress_ptr->set_tooltip(std::to_string(static_cast<int>(v * 100)) + "%");
    });

    return app.run();
}
