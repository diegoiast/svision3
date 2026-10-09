// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/plugin.hpp"
#include "svision3/menu.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace svision3 {

PluginManager::~PluginManager() {
    remove_all();
    plugins_.clear();
    // Menu items handed out by make_menu() may outlive us; this is how they find
    // out. Cleared last, so nothing during teardown sees a half-destroyed manager.
    *self_ = nullptr;
}

PluginManager::Entry *PluginManager::find_entry(std::string_view name) {
    auto it = std::find_if(plugins_.begin(), plugins_.end(),
                           [name](auto const &e) { return e.plugin->name() == name; });
    return it == plugins_.end() ? nullptr : &*it;
}

PluginManager::Entry const *PluginManager::find_entry(std::string_view name) const {
    auto it = std::find_if(plugins_.begin(), plugins_.end(),
                           [name](auto const &e) { return e.plugin->name() == name; });
    return it == plugins_.end() ? nullptr : &*it;
}

bool PluginManager::register_plugin(std::shared_ptr<Plugin> plugin) {
    if (!plugin) {
        return false;
    }
    if (find_entry(plugin->name())) {
        spdlog::warn("Plugin '{}' is already registered", plugin->name());
        return false;
    }
    plugins_.push_back({std::move(plugin), false});
    return true;
}

bool PluginManager::install(std::shared_ptr<Plugin> plugin) {
    if (!plugin) {
        return false;
    }
    auto name = std::string(plugin->name());
    return register_plugin(std::move(plugin)) && install(name);
}

bool PluginManager::install(std::string_view name) {
    auto *entry = find_entry(name);
    if (!entry) {
        spdlog::warn("Plugin '{}' is not registered", name);
        return false;
    }
    if (entry->installed) {
        return false;
    }
    entry->plugin->install(host_);
    entry->installed = true;
    spdlog::info("Installed plugin '{}'", name);
    return true;
}

bool PluginManager::remove(std::string_view name) {
    auto *entry = find_entry(name);
    if (!entry || !entry->installed) {
        return false;
    }
    entry->plugin->uninstall(host_);
    entry->installed = false;
    spdlog::info("Removed plugin '{}'", name);
    return true;
}

bool PluginManager::set_installed(std::string_view name, bool installed) {
    if (installed == is_installed(name)) {
        return is_registered(name);
    }
    return installed ? install(name) : remove(name);
}

bool PluginManager::unregister(std::string_view name) {
    auto it = std::find_if(plugins_.begin(), plugins_.end(),
                           [name](auto const &e) { return e.plugin->name() == name; });
    if (it == plugins_.end()) {
        return false;
    }
    // uninstall() first, release second. A plugin living in a shared library needs
    // both to have happened before that library can be unloaded, and doing them in
    // the other order would tear down state uninstall() still needs.
    if (it->installed) {
        it->plugin->uninstall(host_);
    }
    plugins_.erase(it);
    return true;
}

void PluginManager::remove_all() {
    // Reverse order, so a plugin that built on an earlier one comes out first.
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        if (it->installed) {
            it->plugin->uninstall(host_);
            it->installed = false;
        }
    }
}

bool PluginManager::is_installed(std::string_view name) const {
    auto const *entry = find_entry(name);
    return entry && entry->installed;
}

bool PluginManager::is_registered(std::string_view name) const {
    return find_entry(name) != nullptr;
}

std::shared_ptr<Plugin> PluginManager::find(std::string_view name) const {
    auto const *entry = find_entry(name);
    return entry ? entry->plugin : nullptr;
}

std::vector<std::string> PluginManager::installed_names() const {
    auto names = std::vector<std::string>{};
    for (auto const &e : plugins_) {
        if (e.installed) {
            names.emplace_back(e.plugin->name());
        }
    }
    return names;
}

std::vector<std::string> PluginManager::registered_names() const {
    auto names = std::vector<std::string>{};
    names.reserve(plugins_.size());
    for (auto const &e : plugins_) {
        names.emplace_back(e.plugin->name());
    }
    return names;
}

std::shared_ptr<Menu> PluginManager::make_menu(std::string title) {
    auto menu = std::make_shared<Menu>(std::move(title));
    auto items = std::make_shared<std::vector<std::pair<Command::Ptr, std::string>>>();

    for (auto const &entry : plugins_) {
        auto name = std::string(entry.plugin->name());
        auto cmd = Command::create(std::string(entry.plugin->display_name()), nullptr);
        cmd->set_checkable(true);
        cmd->set_checked(entry.installed);

        // Weak: the item lives in a menu the application owns, which may well
        // outlive this manager. A dead manager leaves the item inert.
        cmd->set_execute_func([weak = std::weak_ptr(self_), cmd = cmd.get(), name] {
            auto holder = weak.lock();
            if (!holder || !*holder) {
                return;
            }
            auto *manager = *holder;
            manager->set_installed(name, !manager->is_installed(name));
            cmd->set_checked(manager->is_installed(name));
        });
        items->emplace_back(cmd, name);
        menu->add_action(cmd);
    }

    // A plugin can be switched on or off from somewhere other than this menu --
    // another copy of it, or the application itself -- so the check marks are
    // re-read every time the menu opens rather than only when it is clicked.
    menu->on_about_to_show = [weak = std::weak_ptr(self_), items] {
        auto holder = weak.lock();
        if (!holder || !*holder) {
            return;
        }
        for (auto const &[cmd, name] : *items) {
            cmd->set_checked((*holder)->is_installed(name));
        }
    };
    return menu;
}

} // namespace svision3
