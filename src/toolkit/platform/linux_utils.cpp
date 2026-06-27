// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "linux_utils.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <fontconfig/fontconfig.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <toml++/toml.hpp>
#include <unistd.h>

namespace toolkit::linux_utils {

static SystemFonts detect_kde_fonts() {
    // 10pt is Plasma's compiled-in default when no font= key is written to kdeglobals
    SystemFonts result = {"sans-serif", "monospace", 10};
    const char *home = std::getenv("HOME");
    if (!home) {
        return result;
    }

    std::string path = std::string(home) + "/.config/kdeglobals";
    spdlog::debug("KDE: Attempting to parse fonts from {}", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::debug("KDE: Could not open config file: {}", path);
        return result;
    }

    // INI to valid TOML: quote sections, keys, and values
    std::stringstream ss;
    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            auto section_name = line.substr(1, line.size() - 2);
            ss << "[\"" << section_name << "\"]\n";
            continue;
        }

        auto eq = line.find('=');
        if (eq != std::string::npos) {
            auto key = line.substr(0, eq);
            auto val = line.substr(eq + 1);
            // Trim key and value
            key.erase(key.find_last_not_of(" \t") + 1);
            val.erase(0, val.find_first_not_of(" \t"));

            if (val.size() >= 2 && val.front() == '\"' && val.back() == '\"') {
                val = val.substr(1, val.size() - 2);
            }
            ss << "\"" << key << "\" = \"" << val << "\"\n";
        }
    }

    try {
        auto config = toml::parse(ss.str());
        if (auto gen = config["General"].as_table()) {
            if (auto f = (*gen)["font"].as_string()) {
                std::string val = f->get();
                spdlog::debug("KDE: Found raw system font: '{}' in {}", val, path);
                auto comma = val.find(',');
                if (comma != std::string::npos) {
                    result.system = val.substr(0, comma);
                    auto next_comma = val.find(',', comma + 1);
                    if (next_comma != std::string::npos) {
                        try {
                            result.size = std::stof(val.substr(comma + 1, next_comma - comma - 1));
                        } catch (...) {
                        }
                    }
                } else {
                    result.system = val;
                }
            }
            if (auto f = (*gen)["fixed"].as_string()) {
                std::string val = f->get();
                spdlog::debug("KDE: Found raw fixed font: '{}' in {}", val, path);
                auto comma = val.find(',');
                result.monospace = (comma != std::string::npos) ? val.substr(0, comma) : val;
            }
        }
    } catch (const toml::parse_error &err) {
        spdlog::debug("KDE: toml++ parse error in {}: {} (at line {})", path, err.description(),
                      err.source().begin.line);
    } catch (...) {
        spdlog::debug("KDE: Unknown error parsing {}", path);
    }

    return result;
}

SystemFonts detect_system_fonts() { return detect_kde_fonts(); }

void init_fontconfig() {
    // The conan fontconfig static library has a wrong baked-in prefix, so the
    // relative <include conf.d/> in the system fonts.conf resolves to a
    // non-existent path and alias rules never load.  Probe standard locations
    // and explicitly load them so "sans-serif" etc. resolve correctly.
    static const char *candidates[] = {
        "/etc/fonts/fonts.conf",
        "/usr/local/etc/fonts/fonts.conf",
        nullptr,
    };

    auto *cfg = FcConfigGetCurrent();
    for (auto **c = candidates; *c; ++c) {
        if (access(*c, R_OK) == 0) {
            auto conf = std::string(*c);
            auto conf_d = conf.substr(0, conf.rfind('/')) + "/conf.d";
            FcConfigParseAndLoad(cfg, reinterpret_cast<FcChar8 const *>(conf.c_str()), FcFalse);
            FcConfigParseAndLoad(cfg, reinterpret_cast<FcChar8 const *>(conf_d.c_str()), FcFalse);
            spdlog::debug("fontconfig: loaded {} + conf.d", conf);
            break;
        }
    }

    if (const char *home = std::getenv("HOME")) {
        auto user_fc = std::string(home) + "/.config/fontconfig";
        FcConfigParseAndLoad(cfg, reinterpret_cast<FcChar8 const *>(user_fc.c_str()), FcFalse);
    }

    FcConfigBuildFonts(cfg);
}

} // namespace toolkit::linux_utils
