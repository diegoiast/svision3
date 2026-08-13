cmake_minimum_required(VERSION 3.20)
project(svision3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(spdlog REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(lunasvg REQUIRED)
find_package(CURL REQUIRED)

if(UNIX AND NOT APPLE)
    option(SVISION3_WITH_CAIRO "Build with Cairo support" ON)
else()
    option(SVISION3_WITH_CAIRO "Build with Cairo support" OFF)
endif()
option(SVISION3_BUILD_DEMOS "Build demo applications and tools" ON)
option(SVISION3_BUILD_TASK_MANAGER_DEMO "Build the task_manager demo (fetches hwinfo via FetchContent)" ON)
option(SVISION3_BUILD_TESTS "Build the test suite" ON)
if(UNIX AND NOT APPLE)
    option(SVISION3_WITH_X11 "Build the X11 backend if available" ON)
    option(SVISION3_WITH_WAYLAND "Build the Wayland backend if available" ON)
endif()

find_package(litehtml REQUIRED)
find_package(md4c REQUIRED)

if(SVISION3_WITH_CAIRO)
    find_package(cairo QUIET)
    if(NOT cairo_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
        add_library(cairo::cairo ALIAS PkgConfig::CAIRO)
        pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
        add_library(fontconfig::fontconfig ALIAS PkgConfig::FONTCONFIG)
    endif()
    add_definitions(-DSVISION3_HAS_CAIRO)

    # UNVERIFIED (see docs/design/cairo-shaper-handoff.md): harfbuzz + FreeType
    # for src/svision3/painters/cairo_shaper.cpp (RTL/bidi text shaping). harfbuzz
    # is already a Conan dependency on Linux (conanfile.py); FreeType is expected
    # to come in transitively via cairo's/harfbuzz's own dependencies, but that
    # was never confirmed against a real `conan install` run -- if find_package
    # fails to produce these targets, check build/generators/*.cmake for the
    # actual exported target names first before reaching for pkg-config.
    find_package(harfbuzz QUIET)
    if(NOT harfbuzz_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
        add_library(harfbuzz::harfbuzz ALIAS PkgConfig::HARFBUZZ)
    endif()
    find_package(Freetype QUIET)
    if(NOT Freetype_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FREETYPE2 REQUIRED IMPORTED_TARGET freetype2)
        add_library(Freetype::Freetype ALIAS PkgConfig::FREETYPE2)
    endif()
endif()

if(APPLE)
elseif(UNIX AND NOT APPLE)
    find_package(stb CONFIG REQUIRED)
elseif(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    find_package(OpenGL REQUIRED)
endif()

set(SVISION3_SOURCES
    src/svision3/types.cpp
    src/svision3/painter.cpp
    src/svision3/command.cpp
    src/svision3/xdg_image_loader.cpp
    src/svision3/image_widget.cpp
    src/svision3/lunasvg_image_loader.cpp
    src/svision3/toolbar.cpp
    src/svision3/button.cpp
    src/svision3/button_group.cpp
    src/svision3/button_state.cpp
    src/svision3/checkbox.cpp
    src/svision3/combobox.cpp
    src/svision3/radio_button.cpp
    src/svision3/label.cpp
    src/svision3/progress_bar.cpp
    src/svision3/tab_widget.cpp
    src/svision3/image.cpp
    src/svision3/item_model.cpp
    src/svision3/list_view.cpp
    src/svision3/icon_grid.cpp
    src/svision3/file_browser_widget.cpp
    src/svision3/file_dialog.cpp
    src/svision3/directory_dialog.cpp
    src/svision3/undo_stack.cpp
    src/svision3/slider.cpp
    src/svision3/splitter.cpp
    src/svision3/spin_box.cpp
    src/svision3/line_input.cpp
    src/svision3/line_input_undo.cpp
    src/svision3/text/bidi.cpp
    src/svision3/text/text_layout.cpp
    src/svision3/text_edit.cpp
    src/svision3/table_view.cpp
    src/svision3/tree_view.cpp
    src/svision3/file_tree_model.cpp
    src/svision3/context_menu.cpp
    src/svision3/menu.cpp
    src/svision3/menubar.cpp
    src/svision3/message_box.cpp
    src/svision3/layout.cpp
    src/svision3/dock_area.cpp
    src/svision3/theme.cpp
    src/svision3/theme_base.cpp
    src/svision3/theme_macos.cpp
    src/svision3/theme_factory.cpp
    src/svision3/theme_plasma.cpp
    src/svision3/theme_win11.cpp
    src/svision3/theme_win95.cpp
    src/svision3/window.cpp
    src/svision3/widget.cpp
    src/svision3/widget_loader.cpp
    src/svision3/platform_factory.cpp
    src/svision3/platform/dummy_platform.cpp
    src/svision3/painters/gl_painter.cpp
    src/svision3/painters/gl_offscreen.cpp
    src/svision3/html_view.cpp
    src/svision3/rich_label.cpp
    src/svision3/scroll_area.cpp
    src/svision3/scrollbar.cpp
    src/svision3/scrollable_widget.cpp
    src/svision3/toast_widget.cpp
    src/svision3/tray_icon_builder.cpp
    src/svision3/status_bar.cpp
    src/svision3/window_title_bar.cpp
    src/svision3/net/http.cpp
    src/svision3/charts/area_chart.cpp
    src/svision3/charts/bar_chart.cpp
    src/svision3/charts/candlestick_chart.cpp
    src/svision3/charts/chart_interaction.cpp
    src/svision3/charts/histogram.cpp
    src/svision3/charts/line_chart.cpp
    src/svision3/charts/pie_chart.cpp
    src/svision3/charts/scatter_plot.cpp
    src/svision3/charts/stacked_bar_chart.cpp
)

if(SVISION3_WITH_CAIRO)
    list(APPEND SVISION3_SOURCES src/svision3/painters/cairo_painter.cpp)
    list(APPEND SVISION3_SOURCES src/svision3/painters/cairo_shaper.cpp)
endif()

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    list(APPEND SVISION3_SOURCES
        src/svision3/platform/macos/macos_application_base.mm
        src/svision3/platform/macos/macos_native_platform.mm
        src/svision3/platform/macos/macos_opengl_platform.mm
    )
    if(SVISION3_WITH_CAIRO)
        list(APPEND SVISION3_SOURCES
            src/svision3/platform/macos/macos_cairo_platform.mm)
    endif()
elseif(WIN32)
    list(APPEND SVISION3_SOURCES
        src/svision3/platform/win32/win32_platform.cpp
        src/svision3/platform/win32/win32_image_loader.cpp
        src/svision3/painters/win32_painter.cpp
        src/svision3/painters/win32_shaper.cpp
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    find_package(OpenGL REQUIRED)
    list(APPEND SVISION3_SOURCES
        src/svision3/platform/linux_utils.cpp
        src/svision3/stb_image_loader.cpp
    )

    # --- X11 (optional) ---
    if(SVISION3_WITH_X11)
        find_package(X11 QUIET)
        if(X11_FOUND)
            set(SVISION3_HAS_X11 ON)
            pkg_check_modules(XEXT REQUIRED xext)
            list(APPEND SVISION3_SOURCES
                src/svision3/platform/x11/x11_platform.cpp
            )
        endif()
    endif()

    # --- Wayland (optional) ---
    if(SVISION3_WITH_WAYLAND)
        pkg_check_modules(WAYLAND QUIET wayland-client wayland-cursor wayland-egl egl)
        pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
        pkg_check_modules(XKBCOMMON QUIET xkbcommon)
        find_program(WAYLAND_SCANNER wayland-scanner)
    endif()

    if(SVISION3_WITH_WAYLAND AND WAYLAND_FOUND AND WAYLAND_PROTOCOLS_FOUND AND XKBCOMMON_FOUND AND WAYLAND_SCANNER)
        set(SVISION3_HAS_WAYLAND ON)

        pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
        set(XDG_SHELL_XML ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
        set(FRACTIONAL_SCALE_XML ${WAYLAND_PROTOCOLS_DIR}/staging/fractional-scale/fractional-scale-v1.xml)
        set(VIEWPORTER_XML ${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml)
        set(XDG_DECORATION_XML ${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml)
        set(XDG_DIALOG_XML ${WAYLAND_PROTOCOLS_DIR}/staging/xdg-dialog/xdg-dialog-v1.xml)
        set(CURSOR_SHAPE_XML ${WAYLAND_PROTOCOLS_DIR}/staging/cursor-shape/cursor-shape-v1.xml)
        set(TABLET_XML ${WAYLAND_PROTOCOLS_DIR}/unstable/tablet/tablet-unstable-v2.xml)
        set(POINTER_CONSTRAINTS_XML ${WAYLAND_PROTOCOLS_DIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml)
        set(RELATIVE_POINTER_XML ${WAYLAND_PROTOCOLS_DIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml)
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
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/xdg-decoration-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${XDG_DECORATION_XML} ${WL_GEN_DIR}/xdg-decoration-client-protocol.h
            DEPENDS ${XDG_DECORATION_XML}
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/xdg-decoration-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${XDG_DECORATION_XML} ${WL_GEN_DIR}/xdg-decoration-protocol.c
            DEPENDS ${XDG_DECORATION_XML}
        )

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/xdg-dialog-v1-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${XDG_DIALOG_XML} ${WL_GEN_DIR}/xdg-dialog-v1-client-protocol.h
            DEPENDS ${XDG_DIALOG_XML}
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/xdg-dialog-v1-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${XDG_DIALOG_XML} ${WL_GEN_DIR}/xdg-dialog-v1-protocol.c
            DEPENDS ${XDG_DIALOG_XML}
        )

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/cursor-shape-v1-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${CURSOR_SHAPE_XML} ${WL_GEN_DIR}/cursor-shape-v1-client-protocol.h
            DEPENDS ${CURSOR_SHAPE_XML}
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/cursor-shape-v1-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${CURSOR_SHAPE_XML} ${WL_GEN_DIR}/cursor-shape-v1-protocol.c
            DEPENDS ${CURSOR_SHAPE_XML}
        )

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/tablet-unstable-v2-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${TABLET_XML} ${WL_GEN_DIR}/tablet-unstable-v2-protocol.c
            DEPENDS ${TABLET_XML}
        )

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/pointer-constraints-unstable-v1-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${POINTER_CONSTRAINTS_XML} ${WL_GEN_DIR}/pointer-constraints-unstable-v1-client-protocol.h
            DEPENDS ${POINTER_CONSTRAINTS_XML}
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/pointer-constraints-unstable-v1-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${POINTER_CONSTRAINTS_XML} ${WL_GEN_DIR}/pointer-constraints-unstable-v1-protocol.c
            DEPENDS ${POINTER_CONSTRAINTS_XML}
        )

        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/relative-pointer-unstable-v1-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header
                    ${RELATIVE_POINTER_XML} ${WL_GEN_DIR}/relative-pointer-unstable-v1-client-protocol.h
            DEPENDS ${RELATIVE_POINTER_XML}
        )
        add_custom_command(
            OUTPUT ${WL_GEN_DIR}/relative-pointer-unstable-v1-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code
                    ${RELATIVE_POINTER_XML} ${WL_GEN_DIR}/relative-pointer-unstable-v1-protocol.c
            DEPENDS ${RELATIVE_POINTER_XML}
        )

        list(APPEND SVISION3_SOURCES
            src/svision3/platform/wayland/wl_platform.cpp
            ${WL_GEN_DIR}/xdg-shell-protocol.c
            ${WL_GEN_DIR}/xdg-shell-client-protocol.h
            ${WL_GEN_DIR}/fractional-scale-v1-protocol.c
            ${WL_GEN_DIR}/fractional-scale-v1-client-protocol.h
            ${WL_GEN_DIR}/viewporter-protocol.c
            ${WL_GEN_DIR}/viewporter-client-protocol.h
            ${WL_GEN_DIR}/xdg-decoration-protocol.c
            ${WL_GEN_DIR}/xdg-decoration-client-protocol.h
            ${WL_GEN_DIR}/xdg-dialog-v1-protocol.c
            ${WL_GEN_DIR}/xdg-dialog-v1-client-protocol.h
            ${WL_GEN_DIR}/cursor-shape-v1-protocol.c
            ${WL_GEN_DIR}/cursor-shape-v1-client-protocol.h
            ${WL_GEN_DIR}/tablet-unstable-v2-protocol.c
            ${WL_GEN_DIR}/pointer-constraints-unstable-v1-client-protocol.h
            ${WL_GEN_DIR}/pointer-constraints-unstable-v1-protocol.c
            ${WL_GEN_DIR}/relative-pointer-unstable-v1-client-protocol.h
            ${WL_GEN_DIR}/relative-pointer-unstable-v1-protocol.c
        )
    endif()

    if(NOT SVISION3_HAS_X11 AND NOT SVISION3_HAS_WAYLAND)
        message(FATAL_ERROR "No display backend found. Install X11 or Wayland dev packages.")
    endif()
endif()

add_library(svision3 STATIC ${SVISION3_SOURCES})
add_library(svision3::svision3 ALIAS svision3)
target_include_directories(svision3 PUBLIC include PRIVATE src src/nfd)
target_link_libraries(svision3 PUBLIC spdlog::spdlog tomlplusplus::tomlplusplus litehtml md4c::md4c-html nlohmann_json::nlohmann_json lunasvg::lunasvg CURL::libcurl)

if(NOT MSVC)
    # One function/variable per section, so --gc-sections (enabled per-executable
    # below) can drop unused code instead of only unreferenced whole object files.
    target_compile_options(svision3 PRIVATE -ffunction-sections -fdata-sections)
endif()

if(SVISION3_WITH_CAIRO)
    target_link_libraries(svision3 PUBLIC cairo::cairo)
    if(TARGET fontconfig::fontconfig)
        target_link_libraries(svision3 PUBLIC fontconfig::fontconfig)
    endif()
    # For src/svision3/painters/cairo_shaper.cpp -- see the harfbuzz/Freetype
    # find_package block above and docs/design/cairo-shaper-handoff.md.
    if(TARGET harfbuzz::harfbuzz)
        target_link_libraries(svision3 PUBLIC harfbuzz::harfbuzz)
    endif()
    if(TARGET Freetype::Freetype)
        target_link_libraries(svision3 PUBLIC Freetype::Freetype)
    endif()
endif()

if(APPLE)
    target_link_libraries(svision3 PUBLIC
        "-framework Cocoa" "-framework CoreGraphics"
        "-framework CoreText" "-framework ImageIO" "-framework OpenGL"
        "-framework AppKit" "-framework UniformTypeIdentifiers")
    target_compile_options(svision3 PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_include_directories(svision3 PRIVATE
        src/svision3/platform/macos)
elseif(WIN32)
    target_link_libraries(svision3 PUBLIC user32 gdi32 msimg32 gdiplus opengl32 shlwapi usp10 shell32)
    target_include_directories(svision3 PRIVATE src/svision3/platform/win32)
elseif(UNIX)
    target_include_directories(svision3 PRIVATE ${stb_INCLUDE_DIRS})
    target_link_libraries(svision3 PUBLIC OpenGL::GL stb::stb)
    if(SVISION3_HAS_X11)
        target_link_libraries(svision3 PUBLIC X11::X11 ${XEXT_LIBRARIES})
        target_include_directories(svision3 PRIVATE src/svision3/platform/x11 ${XEXT_INCLUDE_DIRS})
        target_compile_definitions(svision3 PRIVATE SVISION3_HAS_X11)
    endif()

    if(SVISION3_HAS_WAYLAND)
        target_link_libraries(svision3 PUBLIC
            ${WAYLAND_LIBRARIES} ${XKBCOMMON_LIBRARIES})
        target_include_directories(svision3 PRIVATE
            src/svision3/platform/wayland
            ${WAYLAND_INCLUDE_DIRS} ${XKBCOMMON_INCLUDE_DIRS}
            ${WL_GEN_DIR})
        target_compile_definitions(svision3 PRIVATE SVISION3_HAS_WAYLAND)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/svision3/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/svision3/*.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svision3/*.hpp
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

# Bake i18n objects directly into svision3
target_sources(svision3 PRIVATE $<TARGET_OBJECTS:i18n_core> ${I18N_BUILTIN_SRC})

# ── db ────────────────────────────────────────────────────────────────────────
add_library(db OBJECT
    src/svision3/db/registry.cpp
    src/svision3/db/sqlite_connection.cpp
    src/svision3/db/sqlite/sqlite3.c
)
target_include_directories(db PUBLIC include PRIVATE src/svision3/db)
target_compile_features(db PUBLIC cxx_std_20)
if(UNIX)
    target_link_libraries(svision3 PUBLIC pthread dl)
endif()

# Bake db objects directly into svision3
target_sources(svision3 PRIVATE $<TARGET_OBJECTS:db>)

if(SVISION3_BUILD_DEMOS)
    if(SVISION3_BUILD_TASK_MANAGER_DEMO)
        # ── hwinfo (task_manager demo only: live CPU/RAM monitoring) ───────────
        include(FetchContent)
        set(HWINFO_OS OFF CACHE BOOL "" FORCE)
        set(HWINFO_MAINBOARD OFF CACHE BOOL "" FORCE)
        set(HWINFO_CPU ON CACHE BOOL "" FORCE)
        set(HWINFO_DISK OFF CACHE BOOL "" FORCE)
        set(HWINFO_RAM ON CACHE BOOL "" FORCE)
        set(HWINFO_GPU OFF CACHE BOOL "" FORCE)
        set(HWINFO_GPU_OPENCL OFF CACHE BOOL "" FORCE)
        set(HWINFO_BATTERY OFF CACHE BOOL "" FORCE)
        set(HWINFO_NETWORK OFF CACHE BOOL "" FORCE)
        set(HWINFO_SHARED OFF CACHE BOOL "" FORCE)
        set(HWINFO_STATIC ON CACHE BOOL "" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(hwinfo
            GIT_REPOSITORY https://github.com/lfreist/hwinfo.git
            GIT_TAG        88c5072c4a137d54e94c7e712ae28ac284f1dd9b # main, 2026-07-21; pinned for the monitoring/ API
        )
        FetchContent_MakeAvailable(hwinfo)

        # ── process_info (task_manager demo only: process list + network I/O) ──
        # One implementation per platform behind demo/process_info.hpp; Linux
        # and Windows exist today, so macOS gets the empty stub until someone
        # writes a process_info_macos.cpp.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(TASK_MANAGER_PROCESS_INFO_SRC demo/process_info_linux.cpp)
        elseif(WIN32)
            set(TASK_MANAGER_PROCESS_INFO_SRC demo/process_info_win32.cpp)
        else()
            set(TASK_MANAGER_PROCESS_INFO_SRC demo/process_info_stub.cpp)
        endif()

        add_executable(task_manager demo/task_manager.cpp ${TASK_MANAGER_PROCESS_INFO_SRC})
        set_target_properties(task_manager PROPERTIES WIN32_EXECUTABLE ON)
        if(WIN32)
            set_target_properties(task_manager PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
        endif()
        target_link_libraries(task_manager PRIVATE svision3 lfreist-hwinfo::hwinfo)
        if(WIN32)
            # process_info_win32.cpp: psapi = GetProcessMemoryInfo,
            # iphlpapi = GetIfTable2, advapi32 = the token/SID lookup behind
            # ProcessDetail::user.
            target_link_libraries(task_manager PRIVATE psapi iphlpapi advapi32)
        endif()
        if(NOT MSVC)
            target_link_options(task_manager PRIVATE -Wl,--gc-sections)
        endif()
    endif()

    add_executable(demo demo/demo1.cpp)
    set_target_properties(demo PROPERTIES WIN32_EXECUTABLE ON)
    if(WIN32)
        set_target_properties(demo PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
    target_link_libraries(demo PRIVATE svision3)

    add_executable(demo_declarative demo/demo_declarative.cpp)
    set_target_properties(demo_declarative PROPERTIES WIN32_EXECUTABLE ON)
    if(WIN32)
        set_target_properties(demo_declarative PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
    target_link_libraries(demo_declarative PRIVATE svision3)

    add_executable(stock_viewer demo/stock_viewer.cpp)
    set_target_properties(stock_viewer PROPERTIES WIN32_EXECUTABLE ON)
    if(WIN32)
        set_target_properties(stock_viewer PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
    target_link_libraries(stock_viewer PRIVATE svision3)

    if(NOT MSVC)
        # Drop unreferenced sections from libsvision3.a (and any statically-linked
        # deps) at link time -- the library itself stays complete, this only
        # trims what each demo doesn't actually use.
        target_link_options(demo PRIVATE -Wl,--gc-sections)
        target_link_options(demo_declarative PRIVATE -Wl,--gc-sections)
        target_link_options(stock_viewer PRIVATE -Wl,--gc-sections)
    endif()
    add_executable(demo_dock demo/demo_dock.cpp)
    set_target_properties(demo_dock PROPERTIES WIN32_EXECUTABLE ON)
    if(WIN32)
        set_target_properties(demo_dock PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
    target_link_libraries(demo_dock PRIVATE svision3)

    # Scratch tool for the documentation site: renders each widget offscreen
    # (dummy platform + Cairo image surface, no display needed) to a PNG.
    # Not WIN32_EXECUTABLE -- it's a console tool, not a demo app.
    add_executable(widget_screenshot tools/widget_screenshot.cpp)
    target_include_directories(widget_screenshot PRIVATE src)
    target_link_libraries(widget_screenshot PRIVATE svision3)
endif()

# ── nfd (Native File Dialog) ──────────────────────────────────────────────────
if(APPLE)
    enable_language(OBJC)
    add_library(nfd OBJECT src/nfd/nfd_cocoa.m)
    target_compile_options(nfd PRIVATE -fno-objc-arc)

    # No NSStatusItem-based tray icon backend yet -- stub keeps
    # svision3::TrayIcon linkable (build() just returns nullptr) so callers
    # don't need to #ifdef around it. See include/svision3/tray_icon.hpp.
    target_sources(svision3 PRIVATE src/svision3/tray_icon_stub.cpp)
elseif(WIN32)
    add_library(nfd OBJECT src/nfd/nfd_win.cpp)

    # Shell_NotifyIcon-based tray icon, self-contained in the Win32 shell API
    # (shell32, linked above) -- no extra dependency the way the Linux
    # backend needs dbus-1.
    target_sources(svision3 PRIVATE src/svision3/win32/tray_icon.cpp)
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    add_library(nfd OBJECT src/nfd/nfd_portal.cpp)
    target_include_directories(nfd PRIVATE ${DBUS_INCLUDE_DIRS})
    target_link_libraries(svision3 PUBLIC ${DBUS_LIBRARIES})

    # dbus-1 is already a required dependency on Linux (nfd_portal.cpp, above)
    # -- reuse it for the tray icon's own D-Bus service wrapper
    # (svision3::dbus::Service, hosting org.kde.StatusNotifierItem).
    set(SVISION3_HAS_DBUS ON)
    target_sources(svision3 PRIVATE
        src/svision3/linux/dbus_service.cpp
        src/svision3/linux/tray_icon.cpp
    )
    target_include_directories(svision3 PRIVATE ${DBUS_INCLUDE_DIRS})
    target_compile_definitions(svision3 PUBLIC SVISION3_HAS_DBUS)
endif()

# Bake nfd objects directly into svision3 — it is an implementation detail.
target_sources(svision3 PRIVATE $<TARGET_OBJECTS:nfd>)

if(SVISION3_BUILD_DEMOS)
    add_executable(sqlite_viewer demo/sqlite_viewer.cpp)
    set_target_properties(sqlite_viewer PROPERTIES WIN32_EXECUTABLE ON)
    if(WIN32)
        set_target_properties(sqlite_viewer PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
    target_link_libraries(sqlite_viewer PRIVATE svision3)

    add_executable(json_ui_viewer tools/json_ui_viewer.cpp)
    target_link_libraries(json_ui_viewer PRIVATE svision3)

    # Builds everywhere: TrayIcon::create() always links (real backend on
    # Linux and Windows, no-op stub on macOS -- see
    # include/svision3/tray_icon.hpp), and this demo already handles a nullptr
    # result gracefully.
    add_executable(demo_tray demo/demo_tray.cpp)
    set_target_properties(demo_tray PROPERTIES WIN32_EXECUTABLE ON)
    if(WIN32)
        set_target_properties(demo_tray PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
    target_link_libraries(demo_tray PRIVATE svision3)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(SVISION3_BUILD_TESTS)
find_package(Catch2 REQUIRED)
add_executable(tests
    tests/test_bidi.cpp
    tests/test_text_layout.cpp
    tests/test_button_serialization.cpp
    tests/test_catalog.cpp
    tests/test_command.cpp
    tests/test_context_menu.cpp
    tests/test_db.cpp
    tests/test_events.cpp
    tests/test_widget.cpp
    tests/test_button.cpp
    tests/test_label.cpp
    tests/test_checkbox.cpp
    tests/test_combobox.cpp
    tests/test_dock_area.cpp
    tests/test_filter_adapter.cpp
    tests/test_html_view.cpp
    tests/test_icon_grid.cpp
    tests/test_image_widget.cpp
    tests/test_layout.cpp
    tests/test_layout_mouse.cpp
    tests/test_layout_stretch.cpp
    tests/test_line_input.cpp
    tests/test_list_adapter.cpp
    tests/test_list_view.cpp
    tests/test_main.cpp
    tests/test_menubar.cpp
    tests/test_new_image_api.cpp
    tests/test_painter_polygon.cpp
    tests/test_progress_bar.cpp
    tests/test_radio_button.cpp
    tests/test_scroll_area_drag.cpp
    tests/test_slider.cpp
    tests/test_spin_box.cpp
    tests/test_splitter.cpp
    tests/test_tab_widget.cpp
    tests/test_table_view.cpp
    tests/test_text_edit.cpp
    tests/test_toast.cpp
    tests/test_status_bar.cpp
    tests/test_theme.cpp
    tests/test_types.cpp
    tests/test_utf8.cpp
    tests/test_undo_stack.cpp
    tests/test_xdg_image_loader.cpp
    tests/repro_tab_orientation.cpp
    tests/test_window_title_bar.cpp
)
target_link_libraries(tests PRIVATE svision3 Catch2::Catch2)
# A few tests need private headers (e.g. painters/cairo_painter.hpp) that aren't part of the
# public include/ surface, to render off-screen for pixel-level assertions.
target_include_directories(tests PRIVATE src)

if(WIN32)
    target_sources(tests PRIVATE tests/test_win32_shaper.cpp)
endif()
if(SVISION3_HAS_DBUS)
    target_sources(tests PRIVATE tests/test_dbus_service.cpp)
endif()
endif()

# ── Install (for Conan packaging and manual installs) ─────────────────────────
include(GNUInstallDirs)
install(TARGETS svision3
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
