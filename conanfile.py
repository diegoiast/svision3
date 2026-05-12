from conan import ConanFile
from conan.tools.cmake import cmake_layout


class ToolkitRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    options = {"with_cairo": [True, False]}
    default_options = {"with_cairo": False}

    def requirements(self):
        if self.settings.os not in ["Macos", "Windows"] or self.options.with_cairo:
            self.requires("cairo/1.18.0")
        self.requires("spdlog/1.14.1")
        self.requires("catch2/3.7.1")
        self.requires("tomlplusplus/3.4.0")
        self.requires("stb/cci.20240213")
        self.requires("litehtml/0.8")
        self.requires("md4c/0.5.2")

    def layout(self):
        cmake_layout(self, build_folder="build")
