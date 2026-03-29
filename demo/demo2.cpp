// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Declarative UI API demo - exploring what the API could look like.

#include "toolkit/application.hpp"
#include "declarative.hpp"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
    toolkit::Application app;
    auto window = app.create_window("Declarative Demo", {600, 500});

    auto volume_label = ui::label("50%");
    auto group = ui::radio_group();
    auto rootWidget =
        ui::tab_widget()
            .add_tab(
                "Inputs",
                ui::vbox()
                    .add(ui::label("Form Inputs Demo").shrinkable(true))
                    .add(ui::checkbox("Enable feature").checked(true).tooltip("Toggle something"))
                    .add(ui::hbox()
                             .add(ui::label("Name:"))
                             .add(ui::line_input("Type here..."), ui::expand))
                    .add(ui::hbox()
                             .add(ui::label("Password:"))
                             .add(ui::line_input("Password").password_mode(true), ui::expand))
                    .add(ui::hbox()
                             .add(ui::label("Email:"))
                             .add(ui::line_input("email@example.com"), ui::expand))
                    .add(ui::label(""))
                    .add(ui::hbox()
                             .add(ui::spin_box(5, 0, 100, 5))
                             .add(ui::combobox({"Option 1", "Option 2", "Option 3"}).selected(0))))
            .add_tab("Sliders",
                     ui::vbox()
                         .add(ui::label("Sliders Demo").shrinkable(true))
                         .add(ui::hbox()
                                  .add(ui::label("Volume:"))
                                  .add(ui::slider(50, 0, 100)
                                           .on_change([label = volume_label.get()](auto v) {
                                               label->set_text(fmt::format("{:.0f}%", v));
                                           }),
                                       ui::expand)
                                  .add(volume_label))
                         .add(ui::progress_bar(0.5f))
                         .add(ui::label(""))
                         .add(ui::label("Radio Buttons:")))
            .add_tab(
                "Buttons",
                ui::vbox()
                    .add(ui::label("Buttons Demo").shrinkable(true))
                    .add(ui::button("Regular Button").on_click([] {
                        spdlog::info("Regular button");
                    }))
                    .add(ui::button("Disabled Button").enabled(false))
                    .add(ui::button("Tooltip Button").tooltip("This is a tooltip").on_click([] {
                        spdlog::info("Tooltip button");
                    }))
                    .add(ui::checkbox("Checkbox in tab"))
                    .add(ui::checkbox("Another checkbox").checked(true))
                    .add(ui::hbox()
                             .add(ui::radio_button("Option A", group))
                             .add(ui::radio_button("Option B", group))));

    window->set_root(ui::vbox()
                         .margins(ui::no_margins())
                         .add(rootWidget)
                         .add(ui::spacer(), ui::expand)
                         .add(ui::hbox()
                                  .add(ui::button("&Quit").on_click([window] { window->close(); }))
                                  .add(ui::button("About").enabled(false))));

    window->show();
    return app.run();
}
