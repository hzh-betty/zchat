# 寻找核心依赖包
find_package(Drogon REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_package(Libevent CONFIG REQUIRED)

find_package(amqpcpp CONFIG REQUIRED)

find_package(etcd-cpp-apiv3 REQUIRED)
if(TARGET etcd-cpp-apiv3_DEPS_TARGET)
  get_target_property(ZCHAT_ETCD_LINK_LIBRARIES
    etcd-cpp-apiv3_DEPS_TARGET INTERFACE_LINK_LIBRARIES)
  if(ZCHAT_ETCD_LINK_LIBRARIES)
    string(REPLACE "grpc::grpc" "gRPC::grpc++"
      ZCHAT_ETCD_LINK_LIBRARIES "${ZCHAT_ETCD_LINK_LIBRARIES}")
    set_target_properties(etcd-cpp-apiv3_DEPS_TARGET PROPERTIES
      INTERFACE_LINK_LIBRARIES "${ZCHAT_ETCD_LINK_LIBRARIES}")
  endif()
endif()

add_subdirectory(proto)
add_subdirectory(src/common)
add_subdirectory(src/file)
add_subdirectory(src/user)
add_subdirectory(src/friend)
add_subdirectory(src/message)
add_subdirectory(src/transmite)
add_subdirectory(src/speech)
add_subdirectory(src/gateway)
add_subdirectory(tests)
