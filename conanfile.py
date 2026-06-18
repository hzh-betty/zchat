from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain
import os

DEPENDENCY_VERSIONS = {
    "amqp-cpp": "4.3.27",
    "cmake": "4.3.2",
    "drogon": "1.9.13",
    "etcd-cpp-apiv3": "0.15.4",
    "grpc": "1.78.1",
    "libevent": "2.1.12",
    "libsodium": "1.0.22",
    "ninja": "1.13.2",
    "nlohmann_json": "3.12.0",
    "protobuf": "6.33.5",
    "spdlog": "1.17.0",
}

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
        
        "grpc/*:csharp_plugin": False,
        "grpc/*:node_plugin": False,
        "grpc/*:objective_c_plugin": False,
        "grpc/*:php_plugin": False,
        "grpc/*:python_plugin": False,
        "grpc/*:ruby_plugin": False,
        
        "libcurl/*:with_dict": False,
        "libcurl/*:with_file": False,
        "libcurl/*:with_ftp": False,
        "libcurl/*:with_gopher": False,
        "libcurl/*:with_imap": False,
        "libcurl/*:with_mqtt": False,
        "libcurl/*:with_pop3": False,
        "libcurl/*:with_rtsp": False,
        "libcurl/*:with_smtp": False,
        "libcurl/*:with_telnet": False,
        "libcurl/*:with_tftp": False,
        "libcurl/*:with_websockets": False,
        
        
        "libevent/*:with_openssl": True,

        "nlohmann_json/*:header_only": False,
    }

    def requirements(self):
        # 核心直接依赖库
        self.requires(f"drogon/{DEPENDENCY_VERSIONS['drogon']}")
        self.requires(f"amqp-cpp/{DEPENDENCY_VERSIONS['amqp-cpp']}")
        self.requires(f"etcd-cpp-apiv3/{DEPENDENCY_VERSIONS['etcd-cpp-apiv3']}")
        self.requires(f"grpc/{DEPENDENCY_VERSIONS['grpc']}")
        self.requires(f"protobuf/{DEPENDENCY_VERSIONS['protobuf']}")
        self.requires(f"spdlog/{DEPENDENCY_VERSIONS['spdlog']}")
        self.requires(f"libevent/{DEPENDENCY_VERSIONS['libevent']}")
        self.requires(f"libsodium/{DEPENDENCY_VERSIONS['libsodium']}")
        self.requires(f"nlohmann_json/{DEPENDENCY_VERSIONS['nlohmann_json']}")

    def build_requirements(self):
        self.tool_requires(f"cmake/{DEPENDENCY_VERSIONS['cmake']}")
        self.tool_requires(f"ninja/{DEPENDENCY_VERSIONS['ninja']}")

    def layout(self):
        self.folders.source = "."
        self.folders.build = "."
        self.folders.generators = "generators"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)

        # 禁止生成用户预设文件，保持项目根目录整洁
        tc.user_presets_path = False

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        env_targets = os.environ.get("ZCHAT_BUILD_TARGETS", "")
        if env_targets:
            for target in env_targets.split():
                cmake.build(target=target)
        else:
            cmake.build()
