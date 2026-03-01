cmake_minimum_required(VERSION 3.20)
project(toolkit_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(spdlog REQUIRED)

if(WIN32)
    option(TOOLKIT_WITH_CAIRO "Build with Cairo support" OFF)
else()
    option(TOOLKIT_WITH_CAIRO "Build with Cairo support" ON)
endif()

if(APPLE)
    option(TOOLKIT_USE_CAIRO "Enable Cairo rendering backend on macOS" OFF)
    if(TOOLKIT_USE_CAIRO OR TOOLKIT_WITH_CAIRO)
        find_package(cairo REQUIRED)
        add_definitions(-DTOOLKIT_HAS_CAIRO)
        set(HAS_CAIRO ON)
    endif()
elseif(WIN32)
    if(TOOLKIT_WITH_CAIRO)
        find_package(cairo REQUIRED)
        add_definitions(-DTOOLKIT_HAS_CAIRO)
        set(HAS_CAIRO ON)
    endif()
else()
    # Linux/Unix
    find_package(cairo REQUIRED)
    add_definitions(-DTOOLKIT_HAS_CAIRO)
    set(HAS_CAIRO ON)
    find_package(OpenGL REQUIRED)
endif()

set(TOOLKIT_SOURCES
    src/toolkit/painter.cpp
    src/toolkit/button.cpp
    src/toolkit/checkbox.cpp
    src/toolkit/combobox.cpp
    src/toolkit/radio_button.cpp
    src/toolkit/label.cpp
    src/toolkit/progress_bar.cpp
    src/toolkit/tab_widget.cpp
    src/toolkit/list_view.cpp
    src/toolkit/spin_box.cpp
    src/toolkit/line_input.cpp
    src/toolkit/text_edit.cpp
    src/toolkit/table_view.cpp
    src/toolkit/context_menu.cpp
    src/toolkit/layout.cpp
    src/toolkit/theme.cpp
    src/toolkit/window.cpp
    src/toolkit/platform_factory.cpp
    src/toolkit/painters/gl_painter.cpp
)

if(HAS_CAIRO)
    list(APPEND TOOLKIT_SOURCES src/toolkit/painters/cairo_painter.cpp)
endif()

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    list(APPEND TOOLKIT_SOURCES
        src/toolkit/platform/macos/macos_application_base.mm
        src/toolkit/platform/macos/macos_native_platform.mm
        src/toolkit/platform/macos/macos_opengl_platform.mm
    )
    if(TOOLKIT_USE_CAIRO)
        list(APPEND TOOLKIT_SOURCES
            src/toolkit/platform/macos/macos_cairo_platform.mm)
    endif()
elseif(WIN32)
    list(APPEND TOOLKIT_SOURCES
        src/toolkit/platform/win32/win32_platform.cpp
        src/toolkit/painters/win32_painter.cpp
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)

    # --- X11 (optional) ---
    find_package(X11 QUIET)
    if(X11_FOUND)
        set(TOOLKIT_HAS_X11 ON)
        list(APPEND TOOLKIT_SOURCES
            src/toolkit/platform/x11/x11_platform.cpp
        )
    endif()

    # --- Wayland (optional) ---
    pkg_check_modules(WAYLAND QUIET wayland-client wayland-cursor wayland-egl egl)
    pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
    pkg_check_modules(XKBCOMMON QUIET xkbcommon)
    find_program(WAYLAND_SCANNER wayland-scanner)

    if(WAYLAND_FOUND AND WAYLAND_PROTOCOLS_FOUND AND XKBCOMMON_FOUND AND WAYLAND_SCANNER)
        set(TOOLKIT_HAS_WAYLAND ON)

        pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
        set(XDG_SHELL_XML ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
        set(FRACTIONAL_SCALE_XML ${WAYLAND_PROTOCOLS_DIR}/staging/fractional-scale/fractional-scale-v1.xml)
        set(VIEWPORTER_XML ${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml)
        set(WL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland-gen)
        file(MAKE_DIRECTORY ${WL_GEN_DIR})

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/xdg-shell-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${XDG_SHELL_XML} ${WL_GEN_DIR}/xdg-shell-client-protocol.h
            DEPENDS ${XDG_SHELL_XML}
            COMMENT "Generating xdg-shell client header"
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/xdg-shell-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${XDG_SHELL_XML} ${WL_GEN_DIR}/xdg-shell-protocol.c
            DEPENDS ${XDG_SHELL_XML}
            COMMENT "Generating xdg-shell protocol code"
        )

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/fractional-scale-v1-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${FRACTIONAL_SCALE_XML} ${WL_GEN_DIR}/fractional-scale-v1-client-protocol.h
            DEPENDS ${FRACTIONAL_SCALE_XML}
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/fractional-scale-v1-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${FRACTIONAL_SCALE_XML} ${WL_GEN_DIR}/fractional-scale-v1-protocol.c
            DEPENDS ${FRACTIONAL_SCALE_XML}
        )

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/viewporter-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${VIEWPORTER_XML} ${WL_GEN_DIR}/viewporter-client-protocol.h
            DEPENDS ${VIEWPORTER_XML}
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/viewporter-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${VIEWPORTER_XML} ${WL_GEN_DIR}/viewporter-protocol.c
            DEPENDS ${VIEWPORTER_XML}
        )

        list(APPEND TOOLKIT_SOURCES
            src/toolkit/platform/wayland/wl_platform.cpp
            ${WL_GEN_DIR}/xdg-shell-protocol.c
            ${WL_GEN_DIR}/xdg-shell-client-protocol.h
            ${WL_GEN_DIR}/fractional-scale-v1-protocol.c
            ${WL_GEN_DIR}/fractional-scale-v1-client-protocol.h
            ${WL_GEN_DIR}/viewporter-protocol.c
            ${WL_GEN_DIR}/viewporter-client-protocol.h
        )
    endif()

    if(NOT TOOLKIT_HAS_X11 AND NOT TOOLKIT_HAS_WAYLAND)
        message(FATAL_ERROR "No display backend found. Install X11 or Wayland dev packages.")
    endif()
endif()

add_library(toolkit STATIC ${TOOLKIT_SOURCES})
target_include_directories(toolkit PUBLIC include PRIVATE src)
target_link_libraries(toolkit PUBLIC spdlog::spdlog)

if(HAS_CAIRO)
    target_link_libraries(toolkit PUBLIC cairo::cairo)
endif()

if(APPLE)
    target_link_libraries(toolkit PUBLIC
        "-framework Cocoa" "-framework CoreGraphics"
        "-framework CoreText" "-framework ImageIO" "-framework OpenGL")
    target_compile_options(toolkit PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_include_directories(toolkit PRIVATE
        src/toolkit/platform/macos)
elseif(WIN32)
    target_link_libraries(toolkit PUBLIC user32 gdi32 msimg32 gdiplus)
    target_include_directories(toolkit PRIVATE src/toolkit/platform/win32)
elseif(UNIX)
    target_link_libraries(toolkit PUBLIC OpenGL::GL)
    if(TOOLKIT_HAS_X11)
        target_link_libraries(toolkit PUBLIC X11::X11)
        target_include_directories(toolkit PRIVATE src/toolkit/platform/x11)
        target_compile_definitions(toolkit PRIVATE TOOLKIT_HAS_X11)
    endif()

    if(TOOLKIT_HAS_WAYLAND)
        target_link_libraries(toolkit PUBLIC
            ${WAYLAND_LIBRARIES} ${XKBCOMMON_LIBRARIES})
        target_include_directories(toolkit PRIVATE
            src/toolkit/platform/wayland
            ${WAYLAND_INCLUDE_DIRS} ${XKBCOMMON_INCLUDE_DIRS}
            ${WL_GEN_DIR})
        target_compile_definitions(toolkit PRIVATE TOOLKIT_HAS_WAYLAND)
    endif()
endif()

# ── i18n ──────────────────────────────────────────────────────────────────────
# Core catalog (OBJECT lib so it compiles once, used by tools AND the library)
add_library(i18n_core OBJECT src/i18n/catalog.cpp)
target_include_directories(i18n_core PUBLIC include)
target_compile_features(i18n_core PUBLIC cxx_std_20)

# Tools (link against core only — no circular dependency)
add_executable(po2cpp tools/po2cpp.cpp)
target_compile_features(po2cpp PRIVATE cxx_std_20)

add_executable(cpp2po tools/cpp2po.cpp)
target_compile_features(cpp2po PRIVATE cxx_std_20)

add_executable(pomerge tools/pomerge.cpp)
target_link_libraries(pomerge PRIVATE i18n_core)

# ── i18n build pipeline ──────────────────────────────────────────────────────
set(I18N_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/i18n-gen)
file(MAKE_DIRECTORY ${I18N_GEN_DIR})

# Collect source files to scan for tr()/tr_n() calls
file(GLOB_RECURSE I18N_SCAN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/toolkit/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/toolkit/*.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/toolkit/*.hpp
)

# Step 1: Extract .pot from sources
add_custom_command(
    OUTPUT ${I18N_GEN_DIR}/messages.pot
    COMMAND cpp2po -o ${I18N_GEN_DIR}/messages.pot ${I18N_SCAN_SOURCES}
    DEPENDS cpp2po ${I18N_SCAN_SOURCES}
    COMMENT "i18n: extracting translatable strings"
)

# Step 2 + 3: Merge .pot with each .po, then generate C++ from merged files
file(GLOB I18N_PO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/*.po)
set(I18N_MERGED_PO_FILES)

foreach(PO_FILE ${I18N_PO_FILES})
    get_filename_component(LANG ${PO_FILE} NAME_WE)
    set(MERGED_PO ${I18N_GEN_DIR}/${LANG}.po)
    add_custom_command(
        OUTPUT ${MERGED_PO}
        COMMAND pomerge -t ${I18N_GEN_DIR}/messages.pot ${PO_FILE}
                -o ${MERGED_PO}
        DEPENDS pomerge ${PO_FILE} ${I18N_GEN_DIR}/messages.pot
        COMMENT "i18n: merging translations for ${LANG}"
    )
    list(APPEND I18N_MERGED_PO_FILES ${MERGED_PO})
endforeach()

if(I18N_MERGED_PO_FILES)
    add_custom_command(
        OUTPUT ${I18N_GEN_DIR}/builtin_translations.cpp
        COMMAND po2cpp -o ${I18N_GEN_DIR}/builtin_translations.cpp
                ${I18N_MERGED_PO_FILES}
        DEPENDS po2cpp ${I18N_MERGED_PO_FILES}
        COMMENT "i18n: generating built-in translation data"
    )
    set(I18N_BUILTIN_SRC ${I18N_GEN_DIR}/builtin_translations.cpp)
else()
    # No .po files — generate a stub so the library always provides the API
    file(WRITE ${I18N_GEN_DIR}/builtin_translations.cpp
        "// No translations available\n"
        "#include \"i18n/builtin.hpp\"\n"
        "namespace i18n::builtin {\n"
        "std::string_view get(std::string_view) { return {}; }\n"
        "std::vector<std::string_view> languages() { return {}; }\n"
        "} // namespace i18n::builtin\n"
    )
    set(I18N_BUILTIN_SRC ${I18N_GEN_DIR}/builtin_translations.cpp)
endif()

# Full i18n library = core + generated builtin translations
add_library(i18n STATIC $<TARGET_OBJECTS:i18n_core> ${I18N_BUILTIN_SRC})
target_include_directories(i18n PUBLIC include)
target_compile_features(i18n PUBLIC cxx_std_20)

# ── db ────────────────────────────────────────────────────────────────────────
add_library(db STATIC
    src/db/registry.cpp
    src/db/sqlite_connection.cpp
    src/db/sqlite/sqlite3.c
)
target_include_directories(db PUBLIC include PRIVATE src/db)
target_compile_features(db PUBLIC cxx_std_20)
if(UNIX)
    target_link_libraries(db PRIVATE pthread dl)
endif()

add_executable(demo demo/main.cpp)
target_link_libraries(demo PRIVATE toolkit nfd)

# ── nfd (Native File Dialog) ──────────────────────────────────────────────────
if(APPLE)
    enable_language(OBJC)
    add_library(nfd STATIC src/nfd/nfd_cocoa.m)
    target_include_directories(nfd PUBLIC src/nfd)
    target_link_libraries(nfd PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
    target_compile_options(nfd PRIVATE -fno-objc-arc)
elseif(WIN32)
    add_library(nfd STATIC src/nfd/nfd_win.cpp)
    target_include_directories(nfd PUBLIC src/nfd)
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    add_library(nfd STATIC src/nfd/nfd_portal.cpp)
    target_include_directories(nfd PUBLIC src/nfd)
    target_link_libraries(nfd PRIVATE ${DBUS_LIBRARIES})
    target_include_directories(nfd PRIVATE ${DBUS_INCLUDE_DIRS})
endif()

add_executable(sqlite_viewer tools/sqlite_viewer.cpp)
target_link_libraries(sqlite_viewer PRIVATE toolkit db nfd)

# ── Tests ─────────────────────────────────────────────────────────────────────
find_package(Catch2 REQUIRED)
add_executable(tests
    tests/test_types.cpp
    tests/test_events.cpp
    tests/test_theme.cpp
    tests/test_widget.cpp
    tests/test_button.cpp
    tests/test_label.cpp
    tests/test_checkbox.cpp
    tests/test_radio_button.cpp
    tests/test_combobox.cpp
    tests/test_progress_bar.cpp
    tests/test_list_adapter.cpp
    tests/test_list_view.cpp
    tests/test_layout.cpp
    tests/test_tab_widget.cpp
    tests/test_filter_adapter.cpp
    tests/test_spin_box.cpp
    tests/test_command.cpp
    tests/test_table_view.cpp
    tests/test_catalog.cpp
    tests/test_db.cpp
)
target_link_libraries(tests PRIVATE toolkit i18n db Catch2::Catch2WithMain)
