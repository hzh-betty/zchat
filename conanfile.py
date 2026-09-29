import os
import shutil
from pathlib import Path

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain

required_conan_version = ">=2.28"


class ZChatRecipe(ConanFile):
    name = "zchat"
    version = "1.0.0"
    package_type = "application"

    settings = "os", "compiler", "build_type", "arch"
    default_options = {
        "boost/*:bzip2": False,
        "boost/*:zlib": False,
        "boost/*:without_charconv": True,
        "boost/*:without_cobalt": True,
        "boost/*:without_context": True,
        "boost/*:without_contract": True,
        "boost/*:without_coroutine": True,
        "boost/*:without_fiber": True,
        "boost/*:without_graph": True,
        "boost/*:without_iostreams": True,
        "boost/*:without_json": True,
        "boost/*:without_locale": True,
        "boost/*:without_log": True,
        "boost/*:without_math": True,
        "boost/*:without_nowide": True,
        "boost/*:without_program_options": True,
        "boost/*:without_serialization": True,
        "boost/*:without_stacktrace": True,
        "boost/*:without_test": True,
        "boost/*:without_timer": True,
        "boost/*:without_type_erasure": True,
        "boost/*:without_url": True,
        "boost/*:without_wave": True,

        "cpprestsdk/*:with_websockets": False,

        "drogon/*:with_mysql": True,
        "drogon/*:with_redis": True,

        "mariadb-connector-c/*:with_curl": False,

        "grpc/*:csharp_plugin": False,
        "grpc/*:node_plugin": False,
        "grpc/*:objective_c_plugin": False,
        "grpc/*:php_plugin": False,
        "grpc/*:python_plugin": False,
        "grpc/*:ruby_plugin": False,

        "libevent/*:with_openssl": True,

        "nlohmann_json/*:header_only": False,
    }

    requires = (
        "drogon/1.9.13",
        "amqp-cpp/4.3.27",
        "etcd-cpp-apiv3/0.15.4",
        "grpc/1.78.1",
        "protobuf/6.33.5",
        "spdlog/1.17.0",
        "libevent/2.1.12",
        "libsodium/1.0.22",
        "nlohmann_json/3.12.0",
    )
    tool_requires = "cmake/4.3.2", "ninja/1.13.2"

    def layout(self):
        self.folders.source = "."
        self.folders.build = "."
        self.folders.generators = "generators"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        build_folder = Path(self.build_folder)
        dependency_paths = "\n".join(
            sorted(
                f"{dependency.ref}={dependency.package_folder}"
                for dependency in self.dependencies.host.values()
                if dependency.package_folder
            )
        )
        dependency_marker = build_folder / ".conan-dependency-paths"
        previous_paths = dependency_marker.read_text() if dependency_marker.exists() else ""
        if previous_paths != dependency_paths and (build_folder / "CMakeCache.txt").exists():
            self.output.info("Conan package paths changed; refreshing the CMake cache")
            shutil.rmtree(build_folder / "CMakeFiles", ignore_errors=True)
            for filename in ("CMakeCache.txt", "build.ninja", ".ninja_deps", ".ninja_log"):
                (build_folder / filename).unlink(missing_ok=True)

        cmake = CMake(self)
        cmake.configure()
        dependency_marker.write_text(dependency_paths)
        env_targets = os.environ.get("ZCHAT_BUILD_TARGETS", "")
        if env_targets:
            for target in env_targets.split():
                cmake.build(target=target)
        else:
            cmake.build()
