// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/message_box.hpp"
#include "svision3/application.hpp"
#include "svision3/button.hpp"
#include "svision3/events.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/platform.hpp"
#include "svision3/rich_label.hpp"
#include "svision3/window.hpp"
#include "svision3/xdg_icons.hpp"

#include <future>
#include <memory>

namespace svision3 {

MessageBox::MessageBox(Window *parent) : parent_(parent) {}

MessageBox &MessageBox::title(std::string_view t) {
    title_ = std::string(t);
    return *this;
}

MessageBox &MessageBox::message(std::string_view m) {
    message_ = std::string(m);
    return *this;
}

MessageBox &MessageBox::icon(MessageBoxIcon i) {
    icon_ = i;
    return *this;
}

MessageBox &MessageBox::markdown(bool enabled) {
    markdown_ = enabled;
    return *this;
}

MessageBox &MessageBox::buttons(MessageBoxButtons b) {
    buttons_ = b;
    return *this;
}

MessageBox::Future MessageBox::show() {
    auto callback = std::make_shared<Callback>();

    std::string title_str = title_;
    std::string message_str = message_;
    MessageBoxIcon icon = icon_;
    MessageBoxButtons buttons = buttons_;
    bool use_markdown = markdown_;

    auto win = Application::instance().create_window(
        title_str, {400, 160}, {.resizable = false, .minimizable = false, .maximizable = false});
    // The callbacks below are all reachable from the window itself (through its
    // root widget, or on_key directly), so they capture a weak_ptr -- holding
    // the shared_ptr there would be a cycle that never frees the window.
    auto weak_win = std::weak_ptr<Window>(win);
    auto root = std::make_unique<VBoxLayout>();
    root->set_margins({16, 16, 16, 16});
    root->set_spacing(12);

    // ── Message row ──────────────────────────────────────────────────────────
    auto msg_row = std::make_unique<HBoxLayout>();
    msg_row->set_spacing(12);
    msg_row->set_margins({});

    std::string icon_name;
    switch (icon) {
    case MessageBoxIcon::Information:
        icon_name = XDG::IconStatus::dialogInformation;
        break;
    case MessageBoxIcon::Warning:
        icon_name = XDG::IconStatus::dialogWarning;
        break;
    case MessageBoxIcon::Error:
        icon_name = XDG::IconStatus::dialogError;
        break;
    case MessageBoxIcon::Question:
        icon_name = XDG::IconStatus::dialogQuestion;
        break;
    case MessageBoxIcon::None:
        break;
    }

    if (!icon_name.empty()) {
        auto icon_btn = std::make_unique<Button>("");
        icon_btn->set_icon(
            Application::instance().load_icon(icon_name, 32, XDG::IconContexts::status));
        icon_btn->set_enabled(false);
        msg_row->add_widget(std::move(icon_btn));
    }

    if (use_markdown) {
        msg_row->add_widget(std::make_unique<RichLabel>(message_str), 1);
    } else {
        msg_row->add_widget(std::make_unique<Label>(message_str), 1);
    }
    root->add_widget(std::move(msg_row), 1);

    // ── Button row ───────────────────────────────────────────────────────────
    auto btn_row = std::make_unique<HBoxLayout>();
    btn_row->set_spacing(8);
    btn_row->set_margins({});
    btn_row->add_widget(std::make_unique<Label>(""), 1);

    auto make_btn = [&](std::string_view label, MessageBoxResult r) {
        auto btn = std::make_unique<Button>(std::string{label});
        btn->on_click = [weak_win, callback, r] {
            if (*callback) {
                (*callback)(r);
            }
            if (auto win = weak_win.lock()) {
                win->close();
            }
        };
        btn_row->add_widget(std::move(btn));
    };

    switch (buttons) {
    case MessageBoxButtons::Ok:
        make_btn("OK", MessageBoxResult::Ok);
        break;
    case MessageBoxButtons::OkCancel:
        make_btn("OK", MessageBoxResult::Ok);
        make_btn("Cancel", MessageBoxResult::Cancel);
        break;
    case MessageBoxButtons::YesNo:
        make_btn("Yes", MessageBoxResult::Yes);
        make_btn("No", MessageBoxResult::No);
        break;
    case MessageBoxButtons::YesNoCancel:
        make_btn("Yes", MessageBoxResult::Yes);
        make_btn("No", MessageBoxResult::No);
        make_btn("Cancel", MessageBoxResult::Cancel);
        break;
    }

    root->add_widget(std::move(btn_row));
    win->set_root(std::move(root));
    win->resize_to_fit();
    auto fixed_size = win->size();
    win->set_min_size(fixed_size);
    win->set_max_size(fixed_size);
    if (parent_ && parent_->platform_window()) {
        win->platform_window()->set_modal_for(parent_->platform_window());
    }
    win->on_key = [weak_win, callback](KeyEvent const &e) -> bool {
        if (e.type == KeyEvent::Type::Press && e.key == Key::Escape) {
            if (*callback) {
                (*callback)(MessageBoxResult::Cancel);
            }
            if (auto win = weak_win.lock()) {
                win->close();
            }
            return true;
        }
        return false;
    };
    win->show();

    return Future(callback);
}

std::future<MessageBoxResult> MessageBox::show_static(Window *parent, std::string_view title,
                                                      std::string_view message, MessageBoxIcon icon,
                                                      MessageBoxButtons buttons) {
    std::string title_str{title};
    std::string message_str{message};

    auto result = MessageBoxResult::Cancel;
    auto done = false;
    auto win = Application::instance().create_window(
        title_str, {400, 160}, {.resizable = false, .minimizable = false, .maximizable = false});
    // See the async show() above: weak, or the window's own callbacks keep it
    // alive forever.
    auto weak_win = std::weak_ptr<Window>(win);
    auto root = std::make_unique<VBoxLayout>();
    root->set_margins({16, 16, 16, 16});
    root->set_spacing(12);

    // ── Message row ──────────────────────────────────────────────────────────
    auto msg_row = std::make_unique<HBoxLayout>();
    msg_row->set_spacing(12);
    msg_row->set_margins({});

    std::string icon_name;
    switch (icon) {
    case MessageBoxIcon::Information:
        icon_name = XDG::IconStatus::dialogInformation;
        break;
    case MessageBoxIcon::Warning:
        icon_name = XDG::IconStatus::dialogWarning;
        break;
    case MessageBoxIcon::Error:
        icon_name = XDG::IconStatus::dialogError;
        break;
    case MessageBoxIcon::Question:
        icon_name = XDG::IconStatus::dialogQuestion;
        break;
    case MessageBoxIcon::None:
        break;
    }

    if (!icon_name.empty()) {
        auto icon_btn = std::make_unique<Button>("");
        icon_btn->set_icon(
            Application::instance().load_icon(icon_name, 32, XDG::IconContexts::status));
        icon_btn->set_enabled(false);
        msg_row->add_widget(std::move(icon_btn));
    }

    msg_row->add_widget(std::make_unique<Label>(message_str), 1);
    root->add_widget(std::move(msg_row), 1);

    // ── Button row ───────────────────────────────────────────────────────────
    auto btn_row = std::make_unique<HBoxLayout>();
    btn_row->set_spacing(8);
    btn_row->set_margins({});
    btn_row->add_widget(std::make_unique<Label>(""), 1); // right-align buttons

    auto make_btn = [&](std::string_view label, MessageBoxResult r) {
        auto btn = std::make_unique<Button>(std::string{label});
        btn->on_click = [weak_win, &result, &done, r] {
            result = r;
            done = true;
            if (auto win = weak_win.lock()) {
                win->close();
            }
        };
        btn_row->add_widget(std::move(btn));
    };

    switch (buttons) {
    case MessageBoxButtons::Ok:
        make_btn("OK", MessageBoxResult::Ok);
        break;
    case MessageBoxButtons::OkCancel:
        make_btn("OK", MessageBoxResult::Ok);
        make_btn("Cancel", MessageBoxResult::Cancel);
        break;
    case MessageBoxButtons::YesNo:
        make_btn("Yes", MessageBoxResult::Yes);
        make_btn("No", MessageBoxResult::No);
        break;
    case MessageBoxButtons::YesNoCancel:
        make_btn("Yes", MessageBoxResult::Yes);
        make_btn("No", MessageBoxResult::No);
        make_btn("Cancel", MessageBoxResult::Cancel);
        break;
    }

    root->add_widget(std::move(btn_row));
    win->set_root(std::move(root));
    win->resize_to_fit();
    auto fixed_size = win->size();
    win->set_min_size(fixed_size);
    win->set_max_size(fixed_size);
    if (parent && parent->platform_window()) {
        win->platform_window()->set_modal_for(parent->platform_window());
    }
    win->on_key = [weak_win, &result, &done](KeyEvent const &e) -> bool {
        if (e.type == KeyEvent::Type::Press && e.key == Key::Escape) {
            result = MessageBoxResult::Cancel;
            done = true;
            if (auto win = weak_win.lock()) {
                win->close();
            }
            return true;
        }
        return false;
    };
    win->show();

    Application::instance().run_until([&done] { return done; });

    std::promise<MessageBoxResult> p;
    p.set_value(result);
    return p.get_future();
}

std::future<MessageBoxResult> MessageBox::information(Window *parent, std::string_view title,
                                                      std::string_view message) {
    return show_static(parent, title, message, MessageBoxIcon::Information, MessageBoxButtons::Ok);
}

std::future<MessageBoxResult> MessageBox::warning(Window *parent, std::string_view title,
                                                  std::string_view message) {
    return show_static(parent, title, message, MessageBoxIcon::Warning, MessageBoxButtons::Ok);
}

std::future<MessageBoxResult> MessageBox::error(Window *parent, std::string_view title,
                                                std::string_view message) {
    return show_static(parent, title, message, MessageBoxIcon::Error, MessageBoxButtons::Ok);
}

std::future<MessageBoxResult> MessageBox::question(Window *parent, std::string_view title,
                                                   std::string_view message) {
    return show_static(parent, title, message, MessageBoxIcon::Question, MessageBoxButtons::YesNo);
}

} // namespace svision3
