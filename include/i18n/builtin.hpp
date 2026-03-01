// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace i18n::builtin {

std::string_view get(std::string_view lang);
std::vector<std::string_view> languages();

} // namespace i18n::builtin
