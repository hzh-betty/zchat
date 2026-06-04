cmake_minimum_required(VERSION 3.20)

project(zchat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# 将自定义的 cmake 模块路径追加进去（供 Drogon 等查找补丁使用）
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 寻找核心依赖包
find_package(Drogon REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_package(Libevent CONFIG REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin
  HINTS "${grpc_PACKAGE_FOLDER_DEBUG}/bin"
  REQUIRED)

# 使用标准的 find_package 查找 amqpcpp
find_package(amqpcpp CONFIG REQUIRED)

find_package(etcd-cpp-apiv3 REQUIRED)

add_subdirectory(proto)
add_subdirectory(src/common)
add_subdirectory(src/repository)
add_subdirectory(src/file)
add_subdirectory(src/user)
add_subdirectory(src/friend)
add_subdirectory(src/message)
add_subdirectory(src/transmite)
add_subdirectory(src/speech)
add_subdirectory(src/gateway)
add_subdirectory(tests)
