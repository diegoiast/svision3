// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/command.hpp"
#include "svision3/command_source.hpp"
#include "svision3/tab_widget.hpp"
#include <memory>
#include <string>

namespace svision3 {

// A fixed position inside an otherwise static toolbar or menu, holding a
// variable-length run of commands taken from the current command context --
// whichever widget its CommandSource is pointing at.
//
// The anchor never asks what kind of widget that is: it reads Widget::commands()
// through the base class, so a widget type the application has never heard of --
// one arriving from a plugin, say -- contributes to the menus on exactly the same
// terms as a TextEdit does.
//
// Only the slice the anchor itself inserted is ever touched. Static buttons and
// items placed around it in the same toolbar or menu keep their place no matter
// how far the dynamic run grows or shrinks.
class CommandAnchor {
  public:
    // A rule for one separator belonging to the slice, so that a run of dynamic
    // items reads as a group against the static items beside it. The separator is
    // part of the slice: it appears only when the run is non-empty, and goes away
    // with it, instead of being left stranded above nothing.
    enum class Separator { None, Before, After };

    // An empty group matches every command of the context widget; otherwise only
    // those whose Command::group() is exactly `group` appear here. That is what
    // keeps a Cut command out of the File menu without either side knowing about
    // the other.
    //
    // `source` must outlive the anchor.
    CommandAnchor(CommandSource &source, CommandSink &sink, int insert_index,
                  std::string group = "", Separator separator = Separator::None);

    // Convenience for the common "follow the active tab" case: builds and owns a
    // TabCommandSource over `tabs`.
    CommandAnchor(TabWidget &tabs, CommandSink &sink, int insert_index, std::string group = "",
                  Separator separator = Separator::None);

    ~CommandAnchor() = default;

    CommandAnchor(CommandAnchor const &) = delete;
    CommandAnchor &operator=(CommandAnchor const &) = delete;

    // Rebuilds the slice now. Happens automatically on every tab switch; call it
    // directly when the active tab's own command list changes without a switch.
    void refresh();

    int insert_index() const { return insert_index_; }
    std::string const &group() const { return group_; }
    int bound_count() const { return bound_count_; }

  private:
    void connect();

    // Set only by the TabWidget convenience constructor, which has to keep the
    // source it built alive for as long as the anchor uses it.
    std::unique_ptr<CommandSource> owned_source_;
    CommandSource &source_;
    CommandSink &sink_;
    int insert_index_ = 0;
    std::string group_;
    Separator separator_ = Separator::None;
    int bound_count_ = 0;
    Subscription subscription_;
};

} // namespace svision3
