// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"
#include "toolkit/xdg_icons.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace toolkit {

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
};

class XdgImageLoader : public IconProvider {
  public:
    explicit XdgImageLoader();
    explicit XdgImageLoader(std::string_view theme_name);

    auto load(std::string_view icon_name, int size, std::string_view context = "") -> Icon override;
    auto set_theme(std::string_view theme_name) -> void;
    auto theme_name() const -> std::string_view;

  private:
    auto find_icon_path(std::string_view icon_name, int size, std::string_view context)
        -> std::optional<std::string>;
    auto load_theme(std::string_view theme_name) -> std::optional<IconTheme>;
    auto parse_index_theme(std::string_view path) -> std::optional<IconTheme>;

    std::string current_theme;
    std::unique_ptr<IconTheme> theme;
    std::vector<std::filesystem::path> xdg_dirs;
};

} // namespace toolkit
