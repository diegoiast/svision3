#include "i18n/catalog.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

static void usage() {
    std::cerr
        << "Usage: pomerge [options] <base.po> <overlay.po> -o <output.po>\n"
        << "\n"
        << "Merges two .po files. Entries from overlay override base.\n"
        << "The output is the union of both, with overlay winning on\n"
        << "duplicate keys.\n"
        << "\n"
        << "Options:\n"
        << "  -t    Template mode: base is a .pot template (untranslated\n"
        << "        entries are kept). Overlay provides translations.\n";
    std::exit(1);
}

int main(int argc, char *argv[]) {
    std::string base_path, overlay_path, output_path;
    bool template_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "-t") {
            template_mode = true;
        } else if (arg == "-h" || arg == "--help") {
            usage();
        } else if (base_path.empty()) {
            base_path = arg;
        } else if (overlay_path.empty()) {
            overlay_path = arg;
        } else {
            usage();
        }
    }
    if (base_path.empty() || overlay_path.empty() || output_path.empty())
        usage();

    i18n::Catalog cat;
    if (!cat.load(base_path, /*include_untranslated=*/template_mode)) {
        std::cerr << "pomerge: cannot load " << base_path << "\n";
        return 1;
    }

    if (!cat.merge_file(overlay_path)) {
        std::cerr << "pomerge: cannot load " << overlay_path << "\n";
        return 1;
    }

    std::ofstream out(output_path);
    if (!out) {
        std::cerr << "pomerge: cannot write " << output_path << "\n";
        return 1;
    }
    out << cat.to_po_string();

    std::cout << "pomerge: merged " << cat.size() << " entry/entries into "
              << output_path << "\n";
    return 0;
}
