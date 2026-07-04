import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy


class SvisionRecipe(ConanFile):
    name = "svision3"
    version = "0.0.1-dev"
    package_type = "static-library"
    description = "A C++20 GUI toolkit with native platform backends"
    license = "MIT"
    homepage = "https://github.com/diegoiast/svision3"
    url = "https://github.com/diegoiast/svision3"
    topics = ("gui", "toolkit", "cairo", "opengl", "cross-platform")

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "fPIC": [True, False],
        "with_x11": [True, False],
        "with_wayland": [True, False],
    }
    default_options = {
        "fPIC": True,
        "with_x11": True,
        "with_wayland": True,
    }
    generators = "CMakeDeps", "CMakeToolchain"

    exports_sources = (
        "LICENSE",
        "CMakeLists.txt",
        "src/*",
        "include/*",
        "translations/*",
        "tools/*",
        "tests/*",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
        if self.settings.os != "Linux":
            self.options.rm_safe("with_x11")
            self.options.rm_safe("with_wayland")

    def validate(self):
        if (
            self.settings.os == "Linux"
            and not self.options.with_x11
            and not self.options.with_wayland
        ):
            raise ConanInvalidConfiguration(
                "svision3 needs at least one of with_x11 or with_wayland on Linux"
            )

    def requirements(self):
        self.requires("spdlog/1.14.1")
        self.requires("tomlplusplus/3.4.0")
        self.requires("litehtml/0.8")
        self.requires("md4c/0.5.2")
        self.requires("nlohmann_json/3.11.3")
        self.requires("lunasvg/3.5.0")

        if self.settings.os == "Linux":
            self.requires("stb/cci.20240213")
            self.requires("harfbuzz/12.3.0", options={"with_glib": False})
            self.requires("freetype/2.13.3")

    def build_requirements(self):
        self.test_requires("catch2/3.7.1")

    def layout(self):
        cmake_layout(self, build_folder="build")

    def build(self):
        skip_test = self.conf.get("tools.build:skip_test", default=True, check_type=bool)
        variables = {
            "TOOLKIT_BUILD_DEMOS": "OFF",
            "TOOLKIT_BUILD_TESTS": "OFF" if skip_test else "ON",
        }
        if self.settings.os == "Linux":
            variables["TOOLKIT_WITH_X11"] = "ON" if self.options.with_x11 else "OFF"
            variables["TOOLKIT_WITH_WAYLAND"] = "ON" if self.options.with_wayland else "OFF"
        cmake = CMake(self)
        cmake.configure(variables=variables)
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["toolkit"]
        self.cpp_info.includedirs = ["include"]
        if self.settings.os == "Linux":
            # dbus-1 is always needed: nfd's xdg-desktop-portal file dialog
            # backend doesn't depend on the X11/Wayland choice.
            system_libs = ["GL", "dl", "pthread", "cairo", "fontconfig", "dbus-1"]
            if self.options.with_x11:
                system_libs += ["X11", "Xext"]
            if self.options.with_wayland:
                system_libs += ["wayland-client", "wayland-cursor", "wayland-egl", "EGL", "xkbcommon"]
            self.cpp_info.system_libs = system_libs
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = [
                "user32", "gdi32", "msimg32", "gdiplus", "opengl32", "shlwapi", "usp10",
            ]
        elif self.settings.os == "Macos":
            self.cpp_info.frameworks = [
                "Cocoa", "CoreGraphics", "CoreText", "ImageIO", "OpenGL",
                "AppKit", "UniformTypeIdentifiers",
            ]
