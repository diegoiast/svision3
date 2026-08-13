// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/image.hpp"
#include "svision3/xdg_icons.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace svision3 {

struct IconDirectory {
    std::string path;
    int size = 0;
    std::string context;
    std::string type = "fixed";
    int min_size = 0;
    int max_size = 0;
};

struct IconTheme {
    std::string name;
    std::vector<std::string> inherits;
    std::vector<IconDirectory> directories;
    // Directory containing this theme's index.theme (and its icon
    // subdirectories) -- e.g. /usr/share/icons/Adwaita or themes/Faenza.
    // Icon files are resolved relative to this, not assumed to live under a
    // fixed "themes/<name>" path.
    std::filesystem::path base_dir;
};

class XdgImageLoader : public IconProvider {
  public:
    explicit XdgImageLoader();
    explicit XdgImageLoader(std::string_view theme_name);

    // Defaults repeated from the interface (see image.hpp) since virtual default arguments bind
    // to the static type of the call, so calls through a concrete XdgImageLoader need it here
    // too.
    auto load(std::string_view icon_name, int size, std::string_view context = "",
             PixelFormat format = detail::default_pixel_format()) -> Icon override;
    // Load a theme by name: searches XDG_DATA_DIRS/icons/<name> (system
    // themes), then the bundled themes/<name> (relative to the current
    // working directory).
    auto set_theme(std::string_view theme_name) -> void;
    // Load a theme directly from `dir` (must contain index.theme), bypassing
    // name-based search entirely -- for a specific theme directory given as
    // an absolute path, or one resolved relative to the app/executable.
    auto set_theme_path(std::filesystem::path const &dir) -> void;
    auto theme_name() const -> std::string_view;
    // True once set_theme()/the constructor found and parsed an index.theme for
    // the current theme name. False means lookups will fail (no icons load).
    auto theme_loaded() const -> bool { return theme != nullptr; }

  private:
    auto find_icon_path(std::string_view icon_name, int size, std::string_view context)
        -> std::optional<std::string>;
    auto load_theme(std::string_view theme_name) -> std::optional<IconTheme>;
    auto parse_index_theme(std::string_view path) -> std::optional<IconTheme>;

    std::string current_theme;
    std::unique_ptr<IconTheme> theme;
    std::vector<std::filesystem::path> xdg_dirs;
};

} // namespace svision3
