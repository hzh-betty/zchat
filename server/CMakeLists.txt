cmake_minimum_required(VERSION 3.20)

project(zchat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MYSQL_INCLUDE_DIRS /usr/include/mysql CACHE PATH "MySQL include directory"
    FORCE)
set(MYSQL_LIBRARIES /usr/lib/x86_64-linux-gnu/libmysqlclient.so CACHE FILEPATH
    "MySQL client library" FORCE)

find_package(Drogon REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(spdlog REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
pkg_check_modules(AMQPCPP REQUIRED IMPORTED_TARGET amqpcpp)
pkg_check_modules(LIBEVENT REQUIRED IMPORTED_TARGET libevent)
pkg_check_modules(LIBEVENT_PTHREADS REQUIRED IMPORTED_TARGET libevent_pthreads)

set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_FILES
    ${PROTO_DIR}/base.proto
    ${PROTO_DIR}/file.proto
    ${PROTO_DIR}/friend.proto
    ${PROTO_DIR}/gateway.proto
    ${PROTO_DIR}/message.proto
    ${PROTO_DIR}/notify.proto
    ${PROTO_DIR}/speech.proto
    ${PROTO_DIR}/transmite.proto
    ${PROTO_DIR}/user.proto)

set(PROTO_SRCS)
set(PROTO_HDRS)
set(GRPC_SRCS)
set(GRPC_HDRS)
foreach(PROTO_FILE ${PROTO_FILES})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.pb.cc)
  set(PROTO_HDR ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.pb.h)
  set(GRPC_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.cc)
  set(GRPC_HDR ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.h)
  add_custom_command(
      OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
      COMMAND protobuf::protoc
      ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/generated
           --grpc_out=${CMAKE_CURRENT_BINARY_DIR}/generated
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
           -I ${PROTO_DIR}
           --experimental_allow_proto3_optional
           ${PROTO_FILE}
      DEPENDS ${PROTO_FILE}
      VERBATIM)
  list(APPEND PROTO_SRCS ${PROTO_SRC})
  list(APPEND PROTO_HDRS ${PROTO_HDR})
  list(APPEND GRPC_SRCS ${GRPC_SRC})
  list(APPEND GRPC_HDRS ${GRPC_HDR})
endforeach()

add_library(zchat_proto STATIC
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${GRPC_SRCS}
    ${GRPC_HDRS})
target_link_libraries(zchat_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
target_include_directories(zchat_proto
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)

set(SERVICE_CORE_SOURCES
    src/common/config.cc
    src/common/crypto.cc
    src/common/logger.cc
    src/common/notify_publisher.cc
    src/common/proto_mapper.cc
    src/common/protobuf_http.cc
    src/common/runtime.cc
    src/common/session_store.cc
    src/common/uuid.cc
    src/file/file_service.cc
    src/file/orm_file_repository.cc
    src/friend/friend_service.cc
    src/friend/orm_friend_repository.cc
    src/message/message_search_index.cc
    src/message/message_service.cc
    src/message/orm_message_repository.cc
    src/repository/orm_helpers.cc
    src/speech/baidu_speech_recognizer.cc
    src/speech/speech_service.cc
    src/speech/speech_recognizer.cc
    src/transmite/message_queue.cc
    src/transmite/transmite_service.cc
    src/transmite/orm_transmite_repository.cc
    src/user/alibaba_sms_client.cc
    src/user/orm_user_repository.cc
    src/user/sms_client.cc
    src/user/user_service.cc)

add_library(zchat_service_core STATIC ${SERVICE_CORE_SOURCES})
target_link_libraries(zchat_service_core
  PUBLIC
    Drogon::Drogon
    zchat_proto
    spdlog::spdlog
    OpenSSL::Crypto
    PkgConfig::AMQPCPP
    PkgConfig::LIBEVENT
    PkgConfig::LIBEVENT_PTHREADS)
target_include_directories(zchat_service_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

add_executable(zchat_gateway
    src/common/config.cc
    src/common/logger.cc
    src/common/protobuf_http.cc
    src/common/runtime.cc
    src/common/session_store.cc
    src/common/uuid.cc
    src/gateway/gateway_builder.cc
    src/gateway/gateway_context.cc
    src/gateway/gateway_controller.cc
    src/gateway/grpc_service_clients.cc
    src/gateway/gateway_main.cc
    src/gateway/notify_subscriber.cc
    src/gateway/websocket_controller.cc)
target_link_libraries(zchat_gateway PRIVATE Drogon::Drogon zchat_proto spdlog::spdlog)
target_include_directories(zchat_gateway
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

add_executable(zchat_user_service
    src/user/user_builder.cc
    src/user/user_context.cc
    src/user/user_grpc_service.cc
    src/user/user_main.cc)
target_link_libraries(zchat_user_service PRIVATE zchat_service_core)

add_executable(zchat_friend_service
    src/friend/friend_builder.cc
    src/friend/friend_context.cc
    src/friend/friend_grpc_service.cc
    src/friend/friend_main.cc)
target_link_libraries(zchat_friend_service PRIVATE zchat_service_core)

add_executable(zchat_message_service
    src/message/message_builder.cc
    src/message/message_context.cc
    src/message/message_grpc_service.cc
    src/message/message_main.cc)
target_link_libraries(zchat_message_service PRIVATE zchat_service_core)

add_executable(zchat_transmite_service
    src/transmite/transmite_builder.cc
    src/transmite/transmite_context.cc
    src/transmite/transmite_grpc_service.cc
    src/transmite/transmite_main.cc)
target_link_libraries(zchat_transmite_service PRIVATE zchat_service_core)

add_executable(zchat_file_service
    src/file/file_builder.cc
    src/file/file_context.cc
    src/file/file_grpc_service.cc
    src/file/file_main.cc)
target_link_libraries(zchat_file_service PRIVATE zchat_service_core)

add_executable(zchat_speech_service
    src/speech/speech_builder.cc
    src/speech/speech_context.cc
    src/speech/speech_grpc_service.cc
    src/speech/speech_main.cc)
target_link_libraries(zchat_speech_service PRIVATE zchat_service_core)

add_executable(zchat_protocol_test
    tests/protocol_test.cc
    src/common/protobuf_http.cc
    src/common/uuid.cc)
target_link_libraries(zchat_protocol_test PRIVATE Drogon::Drogon zchat_proto)
target_link_libraries(zchat_protocol_test PRIVATE zchat_service_core)
target_include_directories(zchat_protocol_test
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

if(BUILD_TESTING)
  add_test(NAME zchat_protocol_test COMMAND zchat_protocol_test)
endif()

add_executable(zchat_logger_test
    tests/logger_test.cc
    src/common/logger.cc)
target_link_libraries(zchat_logger_test PRIVATE spdlog::spdlog)
target_include_directories(zchat_logger_test
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
if(BUILD_TESTING)
  add_test(NAME zchat_logger_test COMMAND zchat_logger_test)
endif()
