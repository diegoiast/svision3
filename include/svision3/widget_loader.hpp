#pragma once

#include "svision3/widget.hpp"
#include "svision3/window.hpp"
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace svision3 {

class WidgetLoader {
  public:
    // Widgets are shared-owned once a container takes them, so the factory
    // produces the shared_ptr directly rather than a unique_ptr that every
    // container would have to convert (and allocate a control block for) on
    // ingest. A registered factory may still return make_unique -- it converts.
    using WidgetFactory = std::function<std::shared_ptr<Widget>(nlohmann::json const &)>;

    static WidgetLoader &instance();

    void register_widget(std::string_view class_name, WidgetFactory factory);
    std::shared_ptr<Widget> create_widget(nlohmann::json const &j);

    // Loads an entire window state from JSON
    std::shared_ptr<Window> load_window(nlohmann::json const &j);

    void register_all_widgets();

  private:
    std::map<std::string, WidgetFactory> factories_;
};

} // namespace svision3
