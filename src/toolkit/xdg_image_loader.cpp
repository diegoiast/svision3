// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/xdg_image_loader.hpp"
#include "toolkit/platform.hpp"
#include <algorithm>
#include <fstream>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace toolkit {

XdgImageLoader::XdgImageLoader() {
    auto xdg_str = std::getenv("XDG_DATA_DIRS");
    if (xdg_str) {
        std::string_view sv(xdg_str);
        while (!sv.empty()) {
            auto pos = sv.find(':');
            if (pos == std::string_view::npos) {
                xdg_dirs.emplace_back(sv);
                break;
            } else {
                xdg_dirs.emplace_back(sv.substr(0, pos));
                sv = sv.substr(pos + 1);
            }
        }
    }
    if (xdg_dirs.empty()) {
        xdg_dirs.emplace_back("/usr/share");
        xdg_dirs.emplace_back("/usr/local/share");
    }
}

XdgImageLoader::XdgImageLoader(std::string_view theme_name) : XdgImageLoader() {
    set_theme(theme_name);
}

auto XdgImageLoader::set_theme(std::string_view theme_name) -> void {
    current_theme = theme_name;
    auto t = load_theme(current_theme);
    if (t) {
        theme = std::make_unique<IconTheme>(std::move(*t));
    } else {
        theme.reset();
    }
}

auto XdgImageLoader::set_theme_path(std::filesystem::path const &dir) -> void {
    auto index_path = dir / "index.theme";
    auto t = parse_index_theme(index_path.string());
    if (t) {
        t->name = dir.filename().string();
        t->base_dir = dir;
        current_theme = t->name;
        theme = std::make_unique<IconTheme>(std::move(*t));
        spdlog::info("XdgImageLoader: loading theme '{}' from {}", current_theme,
                     std::filesystem::absolute(index_path).string());
    } else {
        current_theme.clear();
        theme.reset();
        spdlog::warn("XdgImageLoader: could not load theme from {}",
                     std::filesystem::absolute(index_path).string());
    }
}

auto XdgImageLoader::theme_name() const -> std::string_view { return current_theme; }

auto XdgImageLoader::load_theme(std::string_view theme_name) -> std::optional<IconTheme> {
    for (const auto &base_dir : xdg_dirs) {
        auto theme_dir = base_dir / "icons" / std::filesystem::path(theme_name);
        auto theme_path = theme_dir / "index.theme";
        if (std::filesystem::exists(theme_path)) {
            spdlog::info("XdgImageLoader: loading theme '{}' from {}", theme_name,
                         theme_path.string());
            auto t = parse_index_theme(theme_path.string());
            if (t) {
                t->base_dir = theme_dir;
                return t;
            }
        }
        auto local_dir = std::filesystem::path("themes") / std::filesystem::path(theme_name);
        auto local_path = local_dir / "index.theme";
        if (std::filesystem::exists(local_path)) {
            auto t = parse_index_theme(local_path.string());
            if (t) {
                t->name = theme_name;
                t->base_dir = local_dir;
                spdlog::info("XdgImageLoader: loading theme '{}' from {}", theme_name,
                             std::filesystem::absolute(local_path).string());
                return t;
            }
        }
    }
    return std::nullopt;
}

auto XdgImageLoader::parse_index_theme(std::string_view path) -> std::optional<IconTheme> {
    std::ifstream file(std::string(path), std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();

    IconTheme t;
    t.name = std::string(path);

    auto lines = std::string_view(text);
    bool in_icon_theme = false;
    IconDirectory current_dir;

    while (!lines.empty()) {
        auto pos = lines.find('\n');
        auto line = (pos == std::string_view::npos) ? lines : lines.substr(0, pos);
        if (pos != std::string_view::npos) {
            lines = lines.substr(pos + 1);
        } else {
            lines = {};
        }

        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line = line.substr(1);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        if (line == "[Icon Theme]") {
            in_icon_theme = true;
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            in_icon_theme = false;
            if (!current_dir.path.empty() && current_dir.size > 0) {
                t.directories.push_back(current_dir);
            }
            current_dir = {};
            current_dir.path = std::string(line.substr(1, line.size() - 2));
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        auto key = line.substr(0, eq_pos);
        auto value = line.substr(eq_pos + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
            key = key.substr(0, key.size() - 1);
        }
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value = value.substr(1);
        }

        if (in_icon_theme) {
            if (key == "Name") {
                t.name = value;
            } else if (key == "Inherits") {
                std::string_view v(value);
                while (!v.empty()) {
                    auto comma = v.find(',');
                    if (comma == std::string_view::npos) {
                        if (!v.empty()) {
                            t.inherits.push_back(std::string(v));
                        }
                        break;
                    } else {
                        t.inherits.push_back(std::string(v.substr(0, comma)));
                        v = v.substr(comma + 1);
                        while (!v.empty() && v.front() == ' ') {
                            v = v.substr(1);
                        }
                    }
                }
            }
        } else {
            if (key == "Size") {
                current_dir.size = std::stoi(std::string(value));
            } else if (key == "Context") {
                current_dir.context = value;
            } else if (key == "Type") {
                current_dir.type = value;
            } else if (key == "MinSize") {
                current_dir.min_size = std::stoi(std::string(value));
            } else if (key == "MaxSize") {
                current_dir.max_size = std::stoi(std::string(value));
            }
        }
    }

    if (!current_dir.path.empty() && current_dir.size > 0) {
        t.directories.push_back(current_dir);
    }

    return t;
}

auto XdgImageLoader::find_icon_path(std::string_view icon_name, int size, std::string_view context)
    -> std::optional<std::string> {
    if (!theme) {
        return std::nullopt;
    }

    auto search_theme = [&](const IconTheme &t, std::string_view name) -> std::optional<std::string> {
        std::string best_path;
        int best_score = std::numeric_limits<int>::max();

        auto context_lower = std::string(context);
        std::transform(context_lower.begin(), context_lower.end(), context_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        for (const auto &dir : t.directories) {
            auto dir_context_lower = dir.context;
            std::transform(dir_context_lower.begin(), dir_context_lower.end(),
                           dir_context_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (!context.empty() && dir_context_lower != context_lower) {
                continue;
            }

            int score = std::numeric_limits<int>::max();
            std::string ext = ".png";

            if (dir.type == "scalable") {
                if (size >= dir.min_size && size <= dir.max_size) {
                    score = 0;
                    ext = ".svg";
                } else {
                    continue;
                }
            } else {
                if (dir.size == size) {
                    score = 0;
                } else if (dir.size > size) {
                    // Prefer smallest larger icon (downscaling)
                    score = dir.size - size;
                } else {
                    // Upscaling is much less preferred
                    score = 1000 + (size - dir.size);
                }
            }

            if (score < best_score ||
                (score == best_score && dir.type != "scalable" && !best_path.empty())) {
                auto base = t.base_dir / dir.path;
                static const std::vector<std::string> extensions = {".svg", ".png"};

                std::string current_path;
                for (const auto &ext : extensions) {
                    auto icon_path = base / (std::string(name) + ext);
                    if (std::filesystem::exists(icon_path)) {
                        current_path = icon_path.string();
                        break;
                    }
                }

                if (!current_path.empty()) {
                    best_score = score;
                    best_path = current_path;
                }

                if (best_score == 0 && dir.type != "scalable" && !best_path.empty()) {
                    break;
                }
            }
        }
        return best_path.empty() ? std::nullopt : std::make_optional(best_path);
    };

    // Try `name` across this theme and its inheritance chain.
    auto search_name = [&](std::string_view name) -> std::optional<std::string> {
        if (auto result = search_theme(*theme, name)) {
            return result;
        }
        for (const auto &inherit_name : theme->inherits) {
            auto inherit_theme = load_theme(inherit_name);
            if (inherit_theme) {
                if (auto result = search_theme(*inherit_theme, name)) {
                    return result;
                }
            }
        }
        return std::nullopt;
    };

    if (auto result = search_name(icon_name)) {
        return result;
    }

    // Many modern themes (e.g. Adwaita) only ship "-symbolic" variants of
    // action/status icons, dropping the plain full-color name entirely.
    if (!icon_name.ends_with("-symbolic")) {
        if (auto result = search_name(std::string(icon_name) + "-symbolic")) {
            return result;
        }
    }

    return std::nullopt;
}

auto XdgImageLoader::load(std::string_view icon_name, int size, std::string_view context) -> Icon {
    auto path_opt = find_icon_path(icon_name, size, context);
    if (!path_opt) {
        return nullptr;
    }

    std::string path = *path_opt;
    // SVG is XML-based. Use the SVG loader for all SVG files.
    if (path.ends_with(".svg")) {
        return detail::current_platform()->get_svg_loader()->load_svg(path, size, size);
    }
    // Fallback to raster loader (STB) for everything else (PNG, etc.)
    auto loader = detail::current_platform()->get_image_loader();
    return loader->load(path);
}

} // namespace toolkit
