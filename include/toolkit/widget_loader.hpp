#pragma once

#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace toolkit {

class WidgetLoader {
  public:
    using WidgetFactory = std::function<std::unique_ptr<Widget>(nlohmann::json const &)>;

    static WidgetLoader &instance();

    void register_widget(std::string_view class_name, WidgetFactory factory);
    std::unique_ptr<Widget> create_widget(nlohmann::json const &j);

    // Loads an entire window state from JSON
    std::unique_ptr<Window> load_window(nlohmann::json const &j);

    void register_all_widgets();

  private:
    std::map<std::string, WidgetFactory> factories_;
};

} // namespace toolkit
