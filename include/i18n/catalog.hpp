// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace i18n {

using PluralRule = std::function<int(int n)>;

class Catalog {
  public:
    bool load(std::string const &path, bool include_untranslated = false);
    bool load_string(std::string_view content, bool include_untranslated = false);

    void merge(Catalog const &other);
    bool merge_file(std::string const &path, bool include_untranslated = false);
    bool merge_string(std::string_view content, bool include_untranslated = false);

    std::string const &language() const { return language_; }
    int nplurals() const { return nplurals_; }

    std::string_view lookup(std::string_view msgid, std::string_view context = {}) const;

    std::string_view lookup_plural(std::string_view msgid, std::string_view msgid_plural, int n,
                                   std::string_view context = {}) const;

    std::string to_po_string() const;

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }

  private:
    struct Entry {
        std::string context;
        std::string msgid;
        std::string msgid_plural;
        std::vector<std::string> msgstr;
    };

    static std::string make_key(std::string_view context, std::string_view msgid);
    bool parse_header(std::string_view header);

    std::unordered_map<std::string, Entry> entries_;
    std::string language_;
    std::string plural_forms_;
    int nplurals_ = 2;
    PluralRule plural_rule_;
};

} // namespace i18n
