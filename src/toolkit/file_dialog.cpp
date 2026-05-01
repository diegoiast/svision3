// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/file_dialog.hpp"
#include "toolkit/application.hpp"
#include "toolkit/file_dialog_widget.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include <cstdlib>

namespace toolkit {

static FileDialog::Result show_dialog(Window *parent, std::string_view title,
                                      std::string_view start_path, std::string_view ok_label) {
    auto promise = std::make_shared<std::promise<std::optional<std::string>>>();
    auto future = promise->get_future();
    auto win = Application::instance().create_window(std::string{title}, {700, 500});
    auto widget = std::make_unique<FileDialogWidget>();
    auto fdp = widget.get();

    if (!start_path.empty()) {
        fdp->set_current_path(std::string{start_path});
    } else {
        auto home = std::getenv("HOME");
        if (!home) {
            home = std::getenv("USERPROFILE");
        }
        fdp->set_current_path(home ? home : ".");
    }
    fdp->set_ok_label(ok_label);
    fdp->on_ok = [promise, fdp, win] {
        promise->set_value(fdp->selected_path());
        win->close();
    };
    fdp->on_cancel = [promise, win] {
        promise->set_value(std::nullopt);
        win->close();
    };

    win->set_root(std::move(widget));
    if (parent && parent->platform_window()) {
        win->platform_window()->set_modal_for(parent->platform_window());
    }
    win->show();
    return future;
}

FileDialog::Result FileDialog::open(Window *parent, std::string_view title,
                                    std::string_view start_path) {
    return show_dialog(parent, title, start_path, "Open");
}

FileDialog::Result FileDialog::save(Window *parent, std::string_view title,
                                    std::string_view start_path) {
    return show_dialog(parent, title, start_path, "Save");
}

} // namespace toolkit
