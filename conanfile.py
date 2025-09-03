
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

class QuantumSimXConan(ConanFile):
    name = "quantum-simx"
    version = "0.6.0"
    license = "MIT"
    url = "https://example.com/quantum-simx"
    description = "Quantum circuit simulator (state-vector & density-matrix)"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "*"

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["quantum_simx"]
