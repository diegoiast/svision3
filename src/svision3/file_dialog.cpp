// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/file_dialog.hpp"
#include "svision3/application.hpp"
#include "svision3/file_browser_widget.hpp"
#include "svision3/platform.hpp"
#include "svision3/window.hpp"
#include <cstdlib>
#include <filesystem>
#include <nfd.h>
#include <spdlog/spdlog.h>

namespace svision3 {

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

FileDialog &FileDialog::use_native(bool v) {
    use_native_ = v;
    return *this;
}

FileDialog::Future FileDialog::open() {
    if (title_.empty()) {
        title_ = "Open File";
    }
    return use_native_ ? show_native(false) : show("Open");
}

FileDialog::Future FileDialog::save() {
    if (title_.empty()) {
        title_ = "Save File";
    }
    return use_native_ ? show_native(true) : show("Save");
}

// ── Custom widget dialog ──────────────────────────────────────────────────────

FileDialog::Future FileDialog::show(std::string_view ok_label) {
    auto callback = std::make_shared<Callback>();
    auto settled = std::make_shared<bool>(false);
    auto win = Application::instance().create_window(title_, {700, 500});
    // on_ok/on_cancel live on the widget the window owns, so they take a weak
    // reference back -- a shared one would be a cycle.
    auto weak_win = std::weak_ptr<Window>(win);
    auto widget = std::make_unique<FileBrowserWidget>();
    widget->set_browser_mode(false);
    auto fdp = widget.get();

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

    fdp->on_ok = [callback, settled, fdp, weak_win] {
        if (*settled) {
            return;
        }
        *settled = true;
        auto path = fdp->selected_path();
        if (auto win = weak_win.lock()) {
            win->close();
        }
        if (*callback) {
            (*callback)(std::move(path));
        }
    };
    fdp->on_cancel = [callback, settled, weak_win] {
        if (*settled) {
            return;
        }
        *settled = true;
        if (auto win = weak_win.lock()) {
            win->close();
        }
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

// ── Native (NFD) dialog ───────────────────────────────────────────────────────

FileDialog::Future FileDialog::show_native(bool is_save) {
    auto callback = std::make_shared<Callback>();

    // Copy all builder state by value so the lambda owns it independently.
    struct NfdFilter {
        std::string label;
        std::string pattern;
    };
    auto filter_data = std::make_shared<std::vector<NfdFilter>>();
    for (auto const &f : filters_) {
        filter_data->push_back({f.label, f.pattern});
    }
    auto start = start_path_;
    auto defname = default_name_;

    // For open dialogs, file_must_exist_ maps to FOS_FILEMUSTEXIST on Windows
    // (IFileOpenDialog, used by NFD) which is ON by default — the platform already
    // enforces it.  We still carry the flag into the lambda for a defensive
    // post-check on platforms where the native dialog may not enforce existence
    // (e.g. a future portal backend that allows typing arbitrary paths).
    // For save dialogs the flag is irrelevant and ignored.
    auto must_exist = !is_save && file_must_exist_;

    // Post to main thread so that .then() is registered before this runs.
    Application::post_to_main_thread([callback, filter_data, start, defname, is_save, must_exist] {
        // Build NFD filter array — pointers into filter_data which we own.
        std::vector<nfdu8filteritem_t> nfd_filters;
        nfd_filters.reserve(filter_data->size());
        for (auto const &f : *filter_data) {
            nfd_filters.push_back({f.label.c_str(), f.pattern.c_str()});
        }

        auto const *filter_ptr = nfd_filters.empty() ? nullptr : nfd_filters.data();
        auto const filter_count = static_cast<nfdfiltersize_t>(nfd_filters.size());
        auto const *default_path = start.empty() ? nullptr : start.c_str();

        NFD_Init();
        nfdu8char_t *out_path = nullptr;
        nfdresult_t result;

        if (is_save) {
            auto const *default_name_ptr = defname.empty() ? nullptr : defname.c_str();
            result = NFD_SaveDialogU8(&out_path, filter_ptr, filter_count, default_path,
                                      default_name_ptr);
        } else {
            result = NFD_OpenDialogU8(&out_path, filter_ptr, filter_count, default_path);
        }

        Result path;
        if (result == NFD_OKAY && out_path) {
            // On Windows, IFileOpenDialog has FOS_FILEMUSTEXIST on by default,
            // so the platform should never hand us a non-existing path for an
            // open dialog.  The same holds for NSOpenPanel (macOS) and the XDG
            // portal (Linux).  If this warning fires, the native backend is
            // behaving unexpectedly and the path is suppressed (treated as cancel).
            if (must_exist && !std::filesystem::exists(out_path)) {
                spdlog::warn("FileDialog: native open dialog returned a non-existing path '{}' "
                             "while file_must_exist is set — suppressing result",
                             out_path);
            } else {
                path = std::string{out_path};
            }
            NFD_FreePathU8(out_path);
        }
        NFD_Quit();

        if (*callback) {
            (*callback)(std::move(path));
        }
    });

    return Future{callback};
}

} // namespace svision3
