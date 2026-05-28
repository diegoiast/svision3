#include "toolkit/widget_loader.hpp"
#include "toolkit/button.hpp"
#include "toolkit/checkbox.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/menubar.hpp"
#include "toolkit/progress_bar.hpp"
#include "toolkit/radio_button.hpp"
#include "toolkit/scroll_area.hpp"
#include "toolkit/slider.hpp"
#include "toolkit/spin_box.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/text_edit.hpp"
#include "toolkit/toolbar.hpp"
#include <spdlog/spdlog.h>

namespace toolkit {

WidgetLoader &WidgetLoader::instance() {
    static WidgetLoader loader;
    static bool registered = false;
    if (!registered) {
        loader.register_all_widgets();
        registered = true;
    }
    return loader;
}

void WidgetLoader::register_widget(std::string_view class_name, WidgetFactory factory) {
    factories_[std::string(class_name)] = factory;
}

void WidgetLoader::register_all_widgets() {
    register_widget("Label", [](nlohmann::json const &j) {
        auto w = std::make_unique<Label>();
        w->from_json(j);
        return w;
    });
    register_widget("Button", [](nlohmann::json const &j) {
        auto w = std::make_unique<Button>("");
        w->from_json(j);
        return w;
    });
    register_widget("Checkbox", [](nlohmann::json const &j) {
        auto w = std::make_unique<Checkbox>("");
        w->from_json(j);
        return w;
    });
    register_widget("RadioButton", [](nlohmann::json const &j) {
        static RadioGroup g;
        auto w = std::make_unique<RadioButton>("", g);
        w->from_json(j);
        return w;
    });
    register_widget("VBoxLayout", [](nlohmann::json const &j) {
        auto w = std::make_unique<VBoxLayout>();
        w->from_json(j);
        return w;
    });
    register_widget("HBoxLayout", [](nlohmann::json const &j) {
        auto w = std::make_unique<HBoxLayout>();
        w->from_json(j);
        return w;
    });
    register_widget("TabWidget", [](nlohmann::json const &j) {
        auto w = std::make_unique<TabWidget>();
        w->from_json(j);
        return w;
    });
    register_widget("ScrollArea", [](nlohmann::json const &j) {
        auto w = std::make_unique<ScrollArea>();
        w->from_json(j);
        return w;
    });
    register_widget("ProgressBar", [](nlohmann::json const &j) {
        auto w = std::make_unique<ProgressBar>();
        w->from_json(j);
        return w;
    });
    register_widget("Slider", [](nlohmann::json const &j) {
        auto w = std::make_unique<Slider>();
        w->from_json(j);
        return w;
    });
    register_widget("SpinBox", [](nlohmann::json const &j) {
        auto w = std::make_unique<SpinBox>();
        w->from_json(j);
        return w;
    });
    register_widget("LineInput", [](nlohmann::json const &j) {
        auto w = std::make_unique<LineInput>();
        w->from_json(j);
        return w;
    });
    register_widget("TextEdit", [](nlohmann::json const &j) {
        auto w = std::make_unique<TextEdit>();
        w->from_json(j);
        return w;
    });
    register_widget("Combobox", [](nlohmann::json const &j) {
        auto w = std::make_unique<Combobox>();
        w->from_json(j);
        return w;
    });
    register_widget("MenuBar", [](nlohmann::json const &j) {
        auto w = std::make_unique<MenuBar>();
        w->from_json(j);
        return w;
    });
    register_widget("Toolbar", [](nlohmann::json const &j) {
        auto w = std::make_unique<Toolbar>();
        w->from_json(j);
        return w;
    });
    register_widget("ToolbarSeparator",
                    [](nlohmann::json const &) { return create_toolbar_separator(); });
}

std::unique_ptr<Widget> WidgetLoader::create_widget(nlohmann::json const &j) {
    if (!j.is_object()) {
        spdlog::error("WidgetLoader: Expected object, got: {}", j.dump());
        return nullptr;
    }
    if (!j.contains("type")) {
        spdlog::error("WidgetLoader: Does not contain type");
        return nullptr;
    }

    if (!j["type"].is_string()) {
        spdlog::error("WidgetLoader: 'type' is not a string: {}", j.dump());
        return nullptr;
    }

    auto class_name = j.at("type").is_string() ? j.at("type").get<std::string>() : "";

    if (class_name.empty() || class_name == "Widget") {
        spdlog::error("WidgetLoader: tried creating widget of type '{}'", class_name);
        return nullptr;
    }

    if (factories_.find(class_name) == factories_.end()) {
        spdlog::error("WidgetLoader: Factory not found for class '{}'", class_name);
        return nullptr;
    }

    auto w = factories_[class_name](j);

    // Handle TabWidget
    if (j.contains("tabs") && j["tabs"].is_array()) {
        if (auto *tw = dynamic_cast<TabWidget *>(w.get())) {
            tw->set_current(
                -1); // Prevent adding to a potentially non-empty widget if from_json did something
            // Actually TabWidget is new here, but just in case.
            for (auto const &tab_j : j["tabs"]) {
                if (tab_j.contains("content")) {
                    auto content = create_widget(tab_j.at("content"));
                    if (content) {
                        tw->add_tab(tab_j.value("title", ""), std::move(content),
                                    tab_j.value("closable", true));
                    }
                }
            }
            if (j.contains("current")) {
                tw->set_current(j["current"]);
            }
        }
    }

    return w;
}

std::unique_ptr<Window> WidgetLoader::load_window(nlohmann::json const &j) {
    auto title = j.value("title", "Loaded Window");
    if (!j.contains("size")) {
        spdlog::error("WidgetLoader: 'size' field missing");
        return nullptr;
    }
    auto size_j = j.at("size");
    if (!size_j.contains("width") || !size_j.contains("height")) {
        spdlog::error("WidgetLoader: 'size' field missing width or height: {}", size_j.dump());
        return nullptr;
    }
    Size size{size_j.at("width").get<float>(), size_j.at("height").get<float>()};
    auto window = std::make_unique<Window>(title, size);
    if (j.contains("root")) {
        window->set_root(create_widget(j["root"]));
    }
    return window;
}

} // namespace toolkit
