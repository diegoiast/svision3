#include "svision3/application.hpp"
#include "svision3/widget_loader.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <json_file>" << std::endl;
        return 1;
    }

    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << "Could not open file: " << argv[1] << std::endl;
        return 1;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return 1;
    }

    svision3::Application app;
    auto window = svision3::WidgetLoader::instance().load_window(j);
    
    if (!window) {
        std::cerr << "Failed to load window from JSON." << std::endl;
        return 1;
    }

    window->resize_to_fit();
    window->relayout();
    window->show();
    return app.run();
}
