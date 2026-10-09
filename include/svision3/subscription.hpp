// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace svision3 {

namespace detail {

// The type-erased face of a Signal's slot list. Subscription is not a template --
// one handle has to be able to hold a registration from any Signal -- so it needs
// some way to reach back into a list whose element type it cannot name. A tiny
// virtual interface does that, and does it without a void * in sight.
class SlotOwner {
  public:
    virtual ~SlotOwner() = default;
    virtual void erase_slot(std::size_t id) = 0;
};

} // namespace detail

// A registration handle. Destroying it disconnects, which is the point: a
// listener that lives in a runtime-loaded plugin must stop being called at the
// exact moment the plugin's objects die, and that is one deadline no plugin
// author can be relied upon to remember. Holding the handle as a member of the
// listening object makes it automatic.
//
// The slot list is referred to weakly rather than by reference or by raw pointer,
// and that is the whole design: a handle may outlive the Signal it came from, and
// must be able to tell. A reference could not express the empty state this needs
// either -- the handle is default-constructible, movable, and resettable.
class Subscription {
  public:
    Subscription() = default;
    ~Subscription() { reset(); }

    Subscription(Subscription &&other) noexcept
        : owner_(std::move(other.owner_)), id_(other.id_) {
        other.owner_.reset();
        other.id_ = 0;
    }

    Subscription &operator=(Subscription &&other) noexcept {
        if (this != &other) {
            reset();
            owner_ = std::move(other.owner_);
            id_ = other.id_;
            other.owner_.reset();
            other.id_ = 0;
        }
        return *this;
    }

    Subscription(Subscription const &) = delete;
    Subscription &operator=(Subscription const &) = delete;

    // Disconnects now. Safe on an empty handle, and safe after the Signal itself
    // has been destroyed -- an expired owner means there is nothing left that
    // could call us, so there is nothing to disconnect from either.
    void reset() {
        if (auto owner = owner_.lock()) {
            owner->erase_slot(id_);
        }
        owner_.reset();
        id_ = 0;
    }

    bool is_active() const { return id_ != 0 && !owner_.expired(); }

  private:
    template <typename...> friend class Signal;

    Subscription(std::shared_ptr<detail::SlotOwner> owner, std::size_t id)
        : owner_(std::move(owner)), id_(id) {}

    std::weak_ptr<detail::SlotOwner> owner_;
    std::size_t id_ = 0;
};

// A multi-listener callback point. The slot list is held by shared_ptr and
// referenced weakly by every Subscription, so handles outliving the signal are
// harmless in both directions.
template <typename... Args> class Signal {
  public:
    using Slot = std::function<void(Args...)>;

    [[nodiscard]] Subscription connect(Slot callback) {
        if (!callback) {
            return {};
        }
        auto id = slots_->next_id++;
        slots_->entries.emplace_back(id, std::move(callback));
        return Subscription{slots_, id};
    }

    void emit(Args... args) const {
        // Listeners may disconnect (or connect) from inside their own callback --
        // an anchor whose owner closes the tab it belongs to does exactly that --
        // so walk a snapshot of the ids and re-look-up each one before calling it.
        auto ids = std::vector<std::size_t>{};
        ids.reserve(slots_->entries.size());
        for (auto const &entry : slots_->entries) {
            ids.push_back(entry.first);
        }

        for (auto id : ids) {
            auto &entries = slots_->entries;
            auto it = std::find_if(entries.begin(), entries.end(),
                                   [id](auto const &entry) { return entry.first == id; });
            if (it != entries.end() && it->second) {
                it->second(args...);
            }
        }
    }

    bool empty() const { return slots_->entries.empty(); }

  private:
    struct Slots final : detail::SlotOwner {
        void erase_slot(std::size_t id) override {
            auto it = std::find_if(entries.begin(), entries.end(),
                                   [id](auto const &entry) { return entry.first == id; });
            if (it != entries.end()) {
                entries.erase(it);
            }
        }

        std::size_t next_id = 1;
        std::vector<std::pair<std::size_t, Slot>> entries;
    };

    std::shared_ptr<Slots> slots_ = std::make_shared<Slots>();
};

} // namespace svision3
