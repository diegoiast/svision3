from conan import ConanFile
from conan.tools.cmake import cmake_layout


class ToolkitRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("spdlog/1.14.1")
        self.requires("catch2/3.7.1")
        self.requires("tomlplusplus/3.4.0")
        self.requires("stb/cci.20240213")
        self.requires("litehtml/0.8")
        self.requires("md4c/0.5.2")
        self.requires("nlohmann_json/3.11.3")
        self.requires("lunasvg/3.5.0")

        if self.settings.os == "Linux":
            self.requires("fribidi/1.0.13")
            self.requires("harfbuzz/12.3.0", options={"with_glib": False})


    def layout(self):
        cmake_layout(self, build_folder="build")
