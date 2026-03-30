// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/tree_view.hpp"
#include <filesystem>
#include <mutex>
#include <thread>

namespace toolkit {

class FileTreeModel : public TreeModel {
  public:
    explicit FileTreeModel(std::string root_path, bool lazy = true);

    int root_count() const override { return static_cast<int>(roots_.size()); }
    TreeNode const &root_at(int index) const override;

    void set_root(std::string path);
    void set_lazy(bool lazy) { lazy_ = lazy; }
    bool is_lazy() const { return lazy_; }

    std::function<void()> on_loading_started;
    std::function<void()> on_loading_finished;

  private:
    void load_directory(TreeNode &node, std::filesystem::path const &path);
    void load_directory_async(TreeNode &node, std::filesystem::path const &path);

    std::vector<TreeNode> roots_;
    std::string root_path_;
    bool lazy_ = true;
    std::atomic<bool> loading_ = false;
    std::mutex mutex_;
};

} // namespace toolkit
