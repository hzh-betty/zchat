from conan import ConanFile
from conan.tools.cmake import cmake_layout

class ZChatRecipe(ConanFile):
    name = "zchat"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        # 核心直接依赖库
        self.requires("drogon/1.9.8")
        self.requires("amqp-cpp/4.3.27")
        self.requires("etcd-cpp-apiv3/0.14.3")
        self.requires("grpc/1.54.3")
        self.requires("spdlog/1.12.0")
        
        # libevent 并非上述库的强制传递依赖，但本项目 message_queue 模块直接需要它，因此在此保留直接声明
        self.requires("libevent/2.1.12")

    def layout(self):
        cmake_layout(self)
