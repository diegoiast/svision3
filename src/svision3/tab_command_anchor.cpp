// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/tab_command_anchor.hpp"
#include "svision3/widget.hpp"
#include <vector>

namespace svision3 {

CommandAnchor::CommandAnchor(CommandSource &source, CommandSink &sink, int insert_index,
                             std::string group, Separator separator)
    : source_(source), sink_(sink), insert_index_(insert_index), group_(std::move(group)),
      separator_(separator) {
    connect();
}

CommandAnchor::CommandAnchor(TabWidget &tabs, CommandSink &sink, int insert_index,
                             std::string group, Separator separator)
    : owned_source_(std::make_unique<TabCommandSource>(tabs)), source_(*owned_source_), sink_(sink),
      insert_index_(insert_index), group_(std::move(group)), separator_(separator) {
    connect();
}

void CommandAnchor::connect() {
    subscription_ = source_.on_context_changed([this] { refresh(); });
    refresh();
}

void CommandAnchor::refresh() {
    // Only the slice this anchor put there last time. Static neighbours, and any
    // other anchor's slice in the same sink, are outside this range and survive.
    sink_.remove_range(insert_index_, bound_count_);
    bound_count_ = 0;

    auto *current = source_.current_context();
    if (!current) {
        return;
    }

    auto matching = std::vector<Command::Ptr>{};
    for (auto const &cmd : current->commands()) {
        if (!cmd) {
            continue;
        }
        if (group_.empty() || cmd->group() == group_) {
            matching.push_back(cmd);
        }
    }
    if (matching.empty()) {
        return;
    }

    auto index = insert_index_;
    auto insert_separator = [&] {
        sink_.insert_separator(index);
        ++index;
        ++bound_count_;
    };

    if (separator_ == Separator::Before) {
        insert_separator();
    }
    for (auto const &cmd : matching) {
        sink_.insert_command(index, cmd);
        ++index;
        ++bound_count_;
    }
    if (separator_ == Separator::After) {
        insert_separator();
    }
}

} // namespace svision3
