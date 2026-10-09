// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace svision3 {

class CommandSource;
class DockArea;
class Menu;
class MenuBar;
class Window;

// What a plugin is handed to work against. The application implements this and
// decides what it is willing to expose -- a plugin gets no way to reach anything
// the host did not hand it.
//
// Every accessor may return null: an application with no dock area, or no
// menubar, is a perfectly good host for plugins that do not need one.
class PluginHost {
  public:
    virtual ~PluginHost() = default;

    virtual Window *window() const = 0;
    virtual DockArea *docks() const = 0;
    virtual MenuBar *menubar() const = 0;

    // The context anything the plugin anchors into a menu or toolbar should
    // follow. Handed over rather than constructed by the plugin, so that a
    // plugin's menus track the same "what is the user working on" the rest of the
    // application already agreed on.
    virtual CommandSource *commands() const = 0;
};

// A component that merges itself into a running application and can be taken back
// out again.
//
// The unmerge half is the one with teeth. A plugin's code and vtables live in a
// shared library that may be unloaded the moment it is removed, so by the time
// uninstall() returns, nothing the plugin created may still be reachable from the
// application: no widgets left in the tree, no menus left on the bar, no
// subscriptions left pointing at its callbacks. Members held as Subscription (see
// subscription.hpp) and as owning pointers handle most of that by simply being
// destroyed -- which is the reason those types are shaped the way they are.
class Plugin {
  public:
    virtual ~Plugin() = default;

    // Stable identity, used to look the plugin up and to keep it from being
    // registered twice.
    virtual std::string_view name() const = 0;

    // What a plugin list shows the user; defaults to the name.
    virtual std::string_view display_name() const { return name(); }

    // These must be re-enterable: a plugin is switched off and on again over its
    // lifetime, and uninstall() is expected to leave it exactly as install() found
    // it, ready to be installed again. Building the plugin's state in install()
    // rather than in its constructor is what makes that fall out for free.
    virtual void install(PluginHost &host) = 0;
    virtual void uninstall(PluginHost &host) = 0;
};

// Owns installed plugins and drives their lifecycle. Installing happens through
// here rather than by calling install() directly, so that removal is guaranteed
// to run in the right order: uninstall() first, destructor second, and only then
// is the plugin's library safe to unload.
//
// Being *registered* and being *installed* are separate states, and a plugin
// stays registered across being switched off and on again.
//
// The separation exists because a plugin cannot own the switch that turns it back
// on: anything it adds in install() is gone after uninstall(), so an enable item
// living there would take itself away. The registry is the application's record
// of what is available, and outlives every install/uninstall cycle.
class PluginManager {
  public:
    explicit PluginManager(PluginHost &host) : host_(host) {}
    ~PluginManager();

    PluginManager(PluginManager const &) = delete;
    PluginManager &operator=(PluginManager const &) = delete;

    // Makes a plugin known, without installing it. Returns false if one of the
    // same name is already registered.
    bool register_plugin(std::shared_ptr<Plugin> plugin);

    // Registers and installs in one step, for a plugin that starts switched on.
    bool install(std::shared_ptr<Plugin> plugin);

    // Switches a registered plugin on or off. The instance is kept either way,
    // so switching it back on reuses it rather than needing a way to rebuild it.
    bool install(std::string_view name);
    bool remove(std::string_view name);
    bool set_installed(std::string_view name, bool installed);

    // Uninstalls and forgets. This is the point at which the plugin's instance is
    // released -- and, for a plugin from a shared library, the point after which
    // that library is safe to unload.
    bool unregister(std::string_view name);

    // Switches everything off, leaving the registry intact.
    void remove_all();

    bool is_installed(std::string_view name) const;
    bool is_registered(std::string_view name) const;
    std::shared_ptr<Plugin> find(std::string_view name) const;
    std::vector<std::string> installed_names() const;
    std::vector<std::string> registered_names() const;

    // A menu of every plugin registered at the time of the call, each item
    // toggling its plugin on or off and showing a check mark while it is on.
    //
    // Built here rather than by any plugin, for the reason above -- and safe to
    // outlive the manager: the items go inert instead of dangling.
    std::shared_ptr<Menu> make_menu(std::string title = "&Plugins");

  private:
    struct Entry {
        std::shared_ptr<Plugin> plugin;
        bool installed = false;
    };

    Entry *find_entry(std::string_view name);
    Entry const *find_entry(std::string_view name) const;

    PluginHost &host_;
    std::vector<Entry> plugins_;

    // Cleared in the destructor, so menu items handed out by make_menu() can tell
    // that the manager they point at is gone.
    std::shared_ptr<PluginManager *> self_ = std::make_shared<PluginManager *>(this);
};

} // namespace svision3
