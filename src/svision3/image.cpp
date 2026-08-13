#include "svision3/image.hpp"
#include "svision3/pixel_format.hpp"

#include <memory>
#include <spdlog/spdlog.h>

namespace svision3 {

static uint32_t parse_color(std::string_view color_str) {
    std::string low_color;
    for (char c : color_str) {
        low_color += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (low_color == "none") {
        return 0x00000000;
    }
    if (color_str.size() >= 7 && color_str[0] == '#') {
        unsigned int r = 0, g = 0, b = 0;
        if (sscanf(std::string(color_str.substr(1, 6)).c_str(), "%02x%02x%02x", &r, &g, &b) == 3) {
            return (r << 24) | (g << 16) | (b << 8) | 0xFF; // RGBA
        }
    }
    return 0xFF00FFFF;
}

std::shared_ptr<ImageData> parse_xpm(std::string_view xpm_data, PixelFormat format) {
    auto img = std::make_shared<ImageData>();
    img->channels = 4;
    img->format = format;

    std::stringstream ss{std::string(xpm_data)};
    std::string line;

    int width = 0, height = 0, num_colors = 0, cpp = 0;
    bool header_found = false;

    while (std::getline(ss, line)) {
        size_t first_quote = line.find('"');
        if (first_quote == std::string::npos) {
            continue;
        }
        size_t last_quote = line.find('"', first_quote + 1);
        if (last_quote == std::string::npos) {
            continue;
        }

        std::string header_str = line.substr(first_quote + 1, last_quote - first_quote - 1);
        std::stringstream header_ss{header_str};
        if (header_ss >> width >> height >> num_colors >> cpp) {
            img->width = width;
            img->height = height;
            img->pixels.resize(width * height * 4);
            header_found = true;
            break;
        }
    }

    if (!header_found) {
        spdlog::error("parse_xpm: header not found");
        return nullptr;
    }

    std::unordered_map<std::string, uint32_t> color_map;
    for (int i = 0; i < num_colors; ++i) {
        if (!std::getline(ss, line)) {
            break;
        }
        size_t first_quote = line.find('"');
        if (first_quote == std::string::npos) {
            i--;
            continue;
        }
        size_t last_quote = line.find('"', first_quote + 1);

        std::string l = line.substr(first_quote + 1, last_quote - first_quote - 1);
        std::string key = l.substr(0, cpp);
        std::string color_def = l.substr(cpp + 3);
        color_map[key] = parse_color(color_def);
    }

    for (int y = 0; y < height; ++y) {
        if (!std::getline(ss, line)) {
            break;
        }
        size_t first_quote = line.find('"');
        if (first_quote == std::string::npos) {
            y--;
            continue;
        }
        size_t last_quote = line.find('"', first_quote + 1);

        std::string row = line.substr(first_quote + 1, last_quote - first_quote - 1);
        for (int x = 0; x < width; ++x) {
            std::string key = row.substr(x * cpp, cpp);
            uint32_t color = color_map[key];

            // parse_color() packs 0xRRGGBBAA; write out B,G,R,A and swap once below if RGBA
            // was requested instead.
            size_t idx = (y * width + x) * 4;
            img->pixels[idx + 0] = (color >> 8) & 0xFF;  // B
            img->pixels[idx + 1] = (color >> 16) & 0xFF; // G
            img->pixels[idx + 2] = (color >> 24) & 0xFF; // R
            img->pixels[idx + 3] = color & 0xFF;         // A
        }
    }

    if (format == PixelFormat::RGBA) {
        pixel::swap_rb(img->pixels.data(), static_cast<size_t>(width) * height);
    }

    return img;
}

} // namespace svision3