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
        self.requires("etcd-cpp-apiv3/0.15.4")
        self.requires("grpc/1.54.3")
        self.requires("spdlog/1.12.0")
        
        self.requires("libevent/2.1.12")

    def layout(self):
        build_type = str(self.settings.build_type).lower()
        build_folder = f"build/conan2-{build_type}"

        self.folders.source = "."
        self.folders.build = build_folder
        self.folders.generators = f"{build_folder}/generators"
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)

        # 禁止生成用户预设文件，避免与项目的CMakePresets.json冲突
        tc.user_presets_path = False

        tc.generate()
