// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "toolkit/image_loader.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace toolkit {

auto ImageLoader::load(std::string_view path) -> Icon {
    int w = 0, h = 0, c = 0;
    auto *data = stbi_load(std::string(path).c_str(), &w, &h, &c, STBI_rgb_alpha);

    if (!data) {
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = w;
    img->height = h;
    img->channels = 4;
    img->pixels.resize(static_cast<size_t>(w) * h * 4);
    std::copy(data, data + img->pixels.size(), img->pixels.begin());
    stbi_image_free(data);

    return img;
}

auto ImageLoader::supported_extensions() const -> std::vector<std::string> {
    return {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".hdr"};
}

XdgImageLoader::XdgImageLoader() : loader(std::make_unique<ImageLoader>()) {
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

auto XdgImageLoader::theme_name() const -> std::string_view { return current_theme; }

auto XdgImageLoader::load_theme(std::string_view theme_name) -> std::optional<IconTheme> {
    for (const auto &base_dir : xdg_dirs) {
        auto theme_path = base_dir / "icons" / std::string(theme_name) / "index.theme";
        if (std::filesystem::exists(theme_path)) {
            return parse_index_theme(theme_path.string());
        }
        auto local_path = std::filesystem::path("themes") / std::string(theme_name) / "index.theme";
        if (std::filesystem::exists(local_path)) {
            auto t = parse_index_theme(local_path.string());
            if (t) {
                t->name = theme_name;
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

    auto search_theme = [&](const IconTheme &t) -> std::optional<std::string> {
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

            if (score < best_score) {
                auto base = std::filesystem::path("themes") / t.name / dir.path;
                auto icon_path = base / (std::string(icon_name) + ext);
                if (std::filesystem::exists(icon_path)) {
                    best_score = score;
                    best_path = icon_path.string();
                    if (score == 0) {
                        break;
                    }
                }
            }
        }
        return best_path.empty() ? std::nullopt : std::make_optional(best_path);
    };

    auto result = search_theme(*theme);
    if (result) {
        return result;
    }

    for (const auto &inherit_name : theme->inherits) {
        auto inherit_theme = load_theme(inherit_name);
        if (inherit_theme) {
            result = search_theme(*inherit_theme);
            if (result) {
                return result;
            }
        }
    }

    return std::nullopt;
}

auto XdgImageLoader::load(std::string_view icon_name, int size, std::string_view context) -> Icon {
    auto path_opt = find_icon_path(icon_name, size, context);
    if (!path_opt) {
        return nullptr;
    }
    return loader->load(*path_opt);
}

} // namespace toolkit
