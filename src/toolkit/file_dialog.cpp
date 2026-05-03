// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/file_dialog.hpp"
#include "toolkit/application.hpp"
#include "toolkit/file_dialog_widget.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include <cstdlib>

namespace toolkit {

FileDialog::FileDialog(Window *parent) : parent_(parent) {}

FileDialog &FileDialog::title(std::string_view t) {
    title_ = t;
    return *this;
}

FileDialog &FileDialog::start_path(std::string_view path) {
    start_path_ = path;
    return *this;
}

FileDialog &FileDialog::default_name(std::string_view name) {
    default_name_ = name;
    return *this;
}

FileDialog &FileDialog::file_must_exist(bool v) {
    file_must_exist_ = v;
    return *this;
}

FileDialog &FileDialog::add_filter(std::string_view label, std::string_view pattern) {
    filters_.push_back({std::string{label}, std::string{pattern}});
    return *this;
}

FileDialog::Future FileDialog::open() {
    if (title_.empty()) {
        title_ = "Open File";
    }
    return show("Open");
}

FileDialog::Future FileDialog::save() {
    if (title_.empty()) {
        title_ = "Save File";
    }
    return show("Save");
}

FileDialog::Future FileDialog::show(std::string_view ok_label) {
    auto callback = std::make_shared<Callback>();
    auto settled  = std::make_shared<bool>(false);
    auto win      = Application::instance().create_window(title_, {700, 500});
    auto widget   = std::make_unique<FileDialogWidget>();
    auto fdp      = widget.get();

    if (!start_path_.empty()) {
        fdp->set_current_path(start_path_);
    } else {
        auto home = std::getenv("HOME");
        if (!home) {
            home = std::getenv("USERPROFILE");
        }
        fdp->set_current_path(home ? home : ".");
    }

    fdp->set_ok_label(ok_label);
    fdp->set_file_must_exist(file_must_exist_);

    if (!default_name_.empty()) {
        fdp->set_filename(default_name_);
    }

    fdp->on_ok = [callback, settled, fdp, win] {
        if (*settled) {
            return;
        }
        *settled = true;
        auto path = fdp->selected_path();
        win->close();
        if (*callback) {
            (*callback)(std::move(path));
        }
    };
    fdp->on_cancel = [callback, settled, win] {
        if (*settled) {
            return;
        }
        *settled = true;
        win->close();
        if (*callback) {
            (*callback)(std::nullopt);
        }
    };

    win->set_root(std::move(widget));
    if (parent_ && parent_->platform_window()) {
        win->platform_window()->set_modal_for(parent_->platform_window());
    }
    win->show();
    return Future{callback};
}

} // namespace toolkit
