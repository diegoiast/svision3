// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/file_tree_model.hpp"
#include "toolkit/tree_view.hpp"

#include <algorithm>
#include <filesystem>
#include <mutex>

namespace toolkit {

FileTreeModel::FileTreeModel(std::string root_path, bool lazy)
    : root_path_(std::move(root_path)), lazy_(lazy) {
    set_root(root_path_);
}

TreeNode const &FileTreeModel::root_at(int index) const {
    static TreeNode empty;
    if (index < 0 || index >= static_cast<int>(roots_.size())) {
        return empty;
    }
    return roots_[index];
}

void FileTreeModel::set_root(std::string path) {
    std::lock_guard<std::mutex> lock(mutex_);

    root_path_ = std::move(path);
    roots_.clear();

    if (!std::filesystem::exists(root_path_) || !std::filesystem::is_directory(root_path_)) {
        return;
    }

    if (lazy_) {
        TreeNode root;
        root.text = std::filesystem::path(root_path_).filename().string();
        if (root.text.empty()) {
            root.text = root_path_;
        }
        root.expanded = false;
        roots_.push_back(std::move(root));
    } else {
        if (on_loading_started) {
            on_loading_started();
        }
        for (auto const &entry : std::filesystem::directory_iterator(root_path_)) {
            TreeNode node;
            node.text = entry.path().filename().string();
            if (entry.is_directory()) {
                load_directory(node, entry.path());
            }
            roots_.push_back(std::move(node));
        }
        std::sort(roots_.begin(), roots_.end(), [](TreeNode const &a, TreeNode const &b) {
            if (a.children.empty() != b.children.empty()) {
                return !a.children.empty();
            }
            return a.text < b.text;
        });
        if (on_loading_finished) {
            on_loading_finished();
        }
    }

    if (on_data_changed) {
        on_data_changed();
    }
}

void FileTreeModel::load_directory(TreeNode &node, std::filesystem::path const &path) {
    try {
        for (auto const &entry : std::filesystem::directory_iterator(path)) {
            TreeNode child;
            child.text = entry.path().filename().string();
            if (entry.is_directory()) {
                load_directory(child, entry.path());
            }
            node.children.push_back(std::move(child));
        }
        std::sort(node.children.begin(), node.children.end(),
                  [](TreeNode const &a, TreeNode const &b) {
                      if (a.children.empty() != b.children.empty()) {
                          return !a.children.empty();
                      }
                      return a.text < b.text;
                  });
    } catch (std::filesystem::filesystem_error const &) {
    }
}

void FileTreeModel::load_directory_async(TreeNode &node, std::filesystem::path const &path) {
    if (on_loading_started) {
        on_loading_started();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loading_ = true;
    }

    TreeNode loaded_node = node;
    load_directory(loaded_node, path);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loading_ = false;
        node.children = std::move(loaded_node.children);
    }

    if (on_loading_finished) {
        on_loading_finished();
    }

    if (on_data_changed) {
        on_data_changed();
    }
}

} // namespace toolkit
