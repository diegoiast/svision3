// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "declarative_raii.hpp"
#include "toolkit/application.hpp"
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
    toolkit::Application app;
    auto window = app.create_window("RAII Demo", {600, 500});

    // Widgets captured by lambdas must be declared before the UI tree.
    auto volume_label = ui::label("50%");
    auto group = ui::radio_group();

    {
        ui::RootScope root(window);
        {
            ui::TabWidgetScope tabs(root);
            {
                ui::TabScope t(tabs, "Inputs");
                t << ui::label("Form Inputs Demo").shrinkable(true);
                t << ui::checkbox("Enable feature").checked(true).tooltip("Toggle something");
                {
                    ui::HBoxScope row(t);
                    row << ui::label("Name:");
                    row << ui::line_input("Type here...").expand();
                }
                {
                    ui::HBoxScope row(t);
                    row << ui::label("Password:");
                    row << ui::line_input("Password").password_mode(true).expand();
                }
                {
                    ui::HBoxScope row(t);
                    row << ui::label("Email:");
                    row << ui::line_input("email@example.com").expand();
                }
                t << ui::label("");
                {
                    ui::HBoxScope row(t);
                    row << ui::spin_box(5, 0, 100, 5);
                    row << ui::combobox({"Option 1", "Option 2", "Option 3"}).selected(0);
                }
            } // "Inputs" tab added to tabs
            {
                ui::TabScope t(tabs, "Sliders");
                t << ui::label("Sliders Demo").shrinkable(true);
                {
                    ui::HBoxScope row(t);
                    row << ui::label("Volume:");
                    row << ui::slider(50, 0, 100)
                               .on_change([vlp = volume_label.get()](auto v) {
                                   vlp->set_text(fmt::format("{:.0f}%", v));
                               })
                               .expand();
                    row << volume_label;
                }
                t << ui::progress_bar(0.5f);
                t << ui::label("");
                t << ui::label("Radio Buttons:");
            } // "Sliders" tab added to tabs
            {
                ui::TabScope t(tabs, "Buttons");
                t << ui::label("Buttons Demo").shrinkable(true);
                t << ui::button("Regular Button").on_click([] { spdlog::info("Regular button"); });
                t << ui::button("Disabled Button").enabled(false);
                t << ui::button("Tooltip Button").tooltip("This is a tooltip").on_click([] {
                    spdlog::info("Tooltip button");
                });
                t << ui::checkbox("Checkbox in tab");
                t << ui::checkbox("Another checkbox").checked(true);
                {
                    ui::HBoxScope row(t);
                    row << ui::radio_button("Option A", group);
                    row << ui::radio_button("Option B", group);
                }
            } // "Buttons" tab added to tabs
        } // tabs added to root
        root << ui::spacer().expand();
        {
            ui::HBoxScope bottom(root);
            bottom << ui::button("&Quit").on_click([window] { window->close(); });
            bottom << ui::button("About").enabled(false);
        } // bottom row added to root
    } // root calls window->set_root()

    window->show();
    return app.run();
}
