// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/file_dialog.hpp"
#include "toolkit/application.hpp"
#include "toolkit/file_dialog_widget.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include <cstdlib>

namespace toolkit {

static FileDialog::Future show_dialog(Window *parent, std::string_view title,
                                      std::string_view start_path, std::string_view ok_label) {
    auto callback = std::make_shared<FileDialog::Callback>();
    auto settled = std::make_shared<bool>(false);
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
    if (parent && parent->platform_window()) {
        win->platform_window()->set_modal_for(parent->platform_window());
    }
    win->show();
    return FileDialog::Future{callback};
}

FileDialog::Future FileDialog::open(Window *parent, std::string_view title,
                                    std::string_view start_path) {
    return show_dialog(parent, title, start_path, "Open");
}

FileDialog::Future FileDialog::save(Window *parent, std::string_view title,
                                    std::string_view start_path) {
    return show_dialog(parent, title, start_path, "Save");
}

} // namespace toolkit
