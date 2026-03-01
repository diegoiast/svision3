#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include "i18n/catalog.hpp"

using namespace i18n;

static const char *FR_PO = R"po(
msgid ""
msgstr ""
"Language: fr\n"
"Plural-Forms: nplurals=2; plural=(n > 1);\n"

msgid "Save"
msgstr "Enregistrer"

msgid "Cancel"
msgstr "Annuler"

msgctxt "file-dialog"
msgid "Open"
msgstr "Ouvrir"

msgctxt "status"
msgid "Open"
msgstr "Ouvert"

msgid "{} item"
msgid_plural "{} items"
msgstr[0] "{} élément"
msgstr[1] "{} éléments"

msgid "Untranslated"
msgstr ""

msgid "Line one\nLine two"
msgstr "Ligne un\nLigne deux"

msgid "Multi "
"line "
"id"
msgstr "Multi "
"ligne "
"id"
)po";

TEST_CASE("Catalog loads from string", "[i18n]") {
    Catalog cat;
    REQUIRE(cat.load_string(FR_PO));
    REQUIRE(cat.language() == "fr");
    REQUIRE(cat.nplurals() == 2);
    REQUIRE_FALSE(cat.empty());
}

TEST_CASE("Simple lookup", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup("Save")) == "Enregistrer");
    REQUIRE(std::string(cat.lookup("Cancel")) == "Annuler");
}

TEST_CASE("Lookup with context", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup("Open", "file-dialog")) == "Ouvrir");
    REQUIRE(std::string(cat.lookup("Open", "status")) == "Ouvert");
}

TEST_CASE("Context-free lookup for contextual string falls back", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup("Open")) == "Open");
}

TEST_CASE("Missing translation falls back to source", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup("Untranslated")) == "Untranslated");
    REQUIRE(std::string(cat.lookup("Not in catalog")) == "Not in catalog");
}

TEST_CASE("French plurals (n > 1)", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup_plural("{} item", "{} items", 0)) == "{} élément");
    REQUIRE(std::string(cat.lookup_plural("{} item", "{} items", 1)) == "{} élément");
    REQUIRE(std::string(cat.lookup_plural("{} item", "{} items", 2)) == "{} éléments");
    REQUIRE(std::string(cat.lookup_plural("{} item", "{} items", 100)) == "{} éléments");
}

TEST_CASE("Plural fallback when not in catalog", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup_plural("unknown singular", "unknown plural", 1)) ==
            "unknown singular");
    REQUIRE(std::string(cat.lookup_plural("unknown singular", "unknown plural", 5)) ==
            "unknown plural");
}

TEST_CASE("Escaped characters", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup("Line one\nLine two")) == "Ligne un\nLigne deux");
}

TEST_CASE("Multi-line string concatenation", "[i18n]") {
    Catalog cat;
    cat.load_string(FR_PO);
    REQUIRE(std::string(cat.lookup("Multi line id")) == "Multi ligne id");
}

static const char *PL_PO = R"po(
msgid ""
msgstr ""
"Language: pl\n"
"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);\n"

msgid "{} file"
msgid_plural "{} files"
msgstr[0] "{} plik"
msgstr[1] "{} pliki"
msgstr[2] "{} plików"
)po";

TEST_CASE("Polish 3-form plurals", "[i18n]") {
    Catalog cat;
    cat.load_string(PL_PO);
    REQUIRE(cat.language() == "pl");
    REQUIRE(cat.nplurals() == 3);

    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 1)) == "{} plik");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 2)) == "{} pliki");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 3)) == "{} pliki");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 4)) == "{} pliki");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 5)) == "{} plików");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 12)) == "{} plików");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 22)) == "{} pliki");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 23)) == "{} pliki");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 25)) == "{} plików");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 112)) == "{} plików");
    REQUIRE(std::string(cat.lookup_plural("{} file", "{} files", 122)) == "{} pliki");
}

TEST_CASE("Load from file", "[i18n]") {
    Catalog cat;
    // Try both paths: from build dir and from project root
    bool ok = cat.load("testdata/fr.po") ||
              cat.load("../../tests/testdata/fr.po") ||
              cat.load("tests/testdata/fr.po");
    REQUIRE(ok);
    REQUIRE(cat.language() == "fr");
    REQUIRE(std::string(cat.lookup("Save")) == "Enregistrer");
    REQUIRE(std::string(cat.lookup("Open", "file-dialog")) == "Ouvrir");
    REQUIRE(std::string(cat.lookup_plural("{} item", "{} items", 5)) == "{} éléments");
}

TEST_CASE("Load nonexistent file fails", "[i18n]") {
    Catalog cat;
    REQUIRE_FALSE(cat.load("nonexistent.po"));
}

TEST_CASE("Empty catalog", "[i18n]") {
    Catalog cat;
    REQUIRE(cat.empty());
    REQUIRE(cat.size() == 0);
    REQUIRE(std::string(cat.lookup("anything")) == "anything");
}

// ── Merge tests ─────────────────────────────────────────────────────────────

static const char *LIB_PO = R"po(
msgid ""
msgstr ""
"Language: fr\n"
"Plural-Forms: nplurals=2; plural=(n > 1);\n"

msgid "OK"
msgstr "OK (lib)"

msgid "Cancel"
msgstr "Annuler (lib)"

msgid "Apply"
msgstr "Appliquer"
)po";

static const char *APP_PO = R"po(
msgid ""
msgstr ""
"Language: fr\n"
"Plural-Forms: nplurals=2; plural=(n > 1);\n"

msgid "Cancel"
msgstr "Annuler (app)"

msgid "Settings"
msgstr "Paramètres"
)po";

TEST_CASE("Merge: app overrides library entries", "[i18n][merge]") {
    Catalog cat;
    cat.load_string(LIB_PO);
    cat.merge_string(APP_PO);

    REQUIRE(std::string(cat.lookup("Cancel")) == "Annuler (app)");
}

TEST_CASE("Merge: library-only entries survive", "[i18n][merge]") {
    Catalog cat;
    cat.load_string(LIB_PO);
    cat.merge_string(APP_PO);

    REQUIRE(std::string(cat.lookup("OK")) == "OK (lib)");
    REQUIRE(std::string(cat.lookup("Apply")) == "Appliquer");
}

TEST_CASE("Merge: app-only entries are added", "[i18n][merge]") {
    Catalog cat;
    cat.load_string(LIB_PO);
    cat.merge_string(APP_PO);

    REQUIRE(std::string(cat.lookup("Settings")) == "Paramètres");
}

TEST_CASE("Merge: total entry count is union", "[i18n][merge]") {
    Catalog cat;
    cat.load_string(LIB_PO);
    REQUIRE(cat.size() == 3);

    cat.merge_string(APP_PO);
    REQUIRE(cat.size() == 4);
}

static const char *APP_PO_2 = R"po(
msgid ""
msgstr ""
"Language: fr\n"
"Plural-Forms: nplurals=2; plural=(n > 1);\n"

msgid "{} item"
msgid_plural "{} items"
msgstr[0] "{} élément (app)"
msgstr[1] "{} éléments (app)"
)po";

TEST_CASE("Merge: plural entries can be overridden", "[i18n][merge]") {
    Catalog cat;
    cat.load_string(FR_PO);
    cat.merge_string(APP_PO_2);

    REQUIRE(std::string(cat.lookup_plural("{} item", "{} items", 1)) == "{} élément (app)");
    REQUIRE(std::string(cat.lookup_plural("{} item", "{} items", 5)) == "{} éléments (app)");
}

TEST_CASE("Merge: three catalogs stack correctly", "[i18n][merge]") {
    Catalog cat;
    cat.load_string(LIB_PO);
    cat.merge_string(APP_PO);

    static const char *PLUGIN_PO = R"po(
msgid ""
msgstr ""
"Language: fr\n"

msgid "OK"
msgstr "OK (plugin)"

msgid "Export"
msgstr "Exporter"
)po";
    cat.merge_string(PLUGIN_PO);

    REQUIRE(std::string(cat.lookup("OK")) == "OK (plugin)");
    REQUIRE(std::string(cat.lookup("Cancel")) == "Annuler (app)");
    REQUIRE(std::string(cat.lookup("Apply")) == "Appliquer");
    REQUIRE(std::string(cat.lookup("Settings")) == "Paramètres");
    REQUIRE(std::string(cat.lookup("Export")) == "Exporter");
    REQUIRE(cat.size() == 5);
}

TEST_CASE("Merge: context entries merge independently", "[i18n][merge]") {
    static const char *BASE = R"po(
msgid ""
msgstr "Language: fr\n"

msgctxt "menu"
msgid "Open"
msgstr "Ouvrir (base)"

msgctxt "button"
msgid "Open"
msgstr "Ouvrir bouton (base)"
)po";
    static const char *OVERLAY = R"po(
msgid ""
msgstr "Language: fr\n"

msgctxt "menu"
msgid "Open"
msgstr "Ouvrir (overlay)"
)po";
    Catalog cat;
    cat.load_string(BASE);
    cat.merge_string(OVERLAY);

    REQUIRE(std::string(cat.lookup("Open", "menu")) == "Ouvrir (overlay)");
    REQUIRE(std::string(cat.lookup("Open", "button")) == "Ouvrir bouton (base)");
}
