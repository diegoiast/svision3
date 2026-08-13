#include "svision3/widget_loader.hpp"
#include "svision3/button.hpp"
#include "svision3/checkbox.hpp"
#include "svision3/combobox.hpp"
#include "svision3/file_browser_widget.hpp"
#include "svision3/image_widget.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/line_input.hpp"
#include "svision3/menubar.hpp"
#include "svision3/progress_bar.hpp"
#include "svision3/radio_button.hpp"
#include "svision3/scroll_area.hpp"
#include "svision3/scrollbar.hpp"
#include "svision3/slider.hpp"
#include "svision3/spin_box.hpp"
#include "svision3/tab_widget.hpp"
#include "svision3/text_edit.hpp"
#include "svision3/toolbar.hpp"
#include <spdlog/spdlog.h>

#define DO_REGISTER_WIDGET(x)                                                                      \
    register_widget(#x, [](nlohmann::json const &j) {                                              \
        auto w = std::make_shared<x>();                                                            \
        w->from_json(j);                                                                           \
        return w;                                                                                  \
    })

namespace svision3 {

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
    // FIXME: simple constructor is missing
    register_widget("Button", [](nlohmann::json const &j) {
        auto w = std::make_shared<Button>("");
        w->from_json(j);
        return w;
    });
    // FIXME: simple constructor is missing
    register_widget("Checkbox", [](nlohmann::json const &j) {
        auto w = std::make_shared<Checkbox>("");
        w->from_json(j);
        return w;
    });
    // FIXME: simple constructor is missing
    register_widget("RadioButton", [](nlohmann::json const &j) {
        // FIXME: support for radio group boxes
        static RadioGroup g;
        auto w = std::make_shared<RadioButton>("", g);
        w->from_json(j);
        return w;
    });
    register_widget("ToolbarSeparator",
                    [](nlohmann::json const &) { return create_toolbar_separator(); });

    DO_REGISTER_WIDGET(Combobox);
    DO_REGISTER_WIDGET(HBoxLayout);
    DO_REGISTER_WIDGET(ImageWidget);
    DO_REGISTER_WIDGET(Label);
    DO_REGISTER_WIDGET(ProgressBar);
    DO_REGISTER_WIDGET(LineInput);
    DO_REGISTER_WIDGET(TextEdit);
    DO_REGISTER_WIDGET(MenuBar);
    DO_REGISTER_WIDGET(ScrollArea);
    DO_REGISTER_WIDGET(Scrollbar);
    DO_REGISTER_WIDGET(Slider);
    DO_REGISTER_WIDGET(TabWidget);
    DO_REGISTER_WIDGET(SpinBox);
    DO_REGISTER_WIDGET(Toolbar);
    DO_REGISTER_WIDGET(VBoxLayout);
    DO_REGISTER_WIDGET(GridLayout);
    DO_REGISTER_WIDGET(FormLayout);
    DO_REGISTER_WIDGET(StackedLayout);
    DO_REGISTER_WIDGET(FileBrowserWidget);
}

std::shared_ptr<Widget> WidgetLoader::create_widget(nlohmann::json const &j) {
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

std::shared_ptr<Window> WidgetLoader::load_window(nlohmann::json const &j) {
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
    auto window = std::make_shared<Window>(title, size);
    if (j.contains("root")) {
        window->set_root(create_widget(j["root"]));
    }
    return window;
}

} // namespace svision3
