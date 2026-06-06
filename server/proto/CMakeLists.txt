set(PROTO_FILES
  base.proto
  file.proto
  friend.proto
  gateway.proto
  message.proto
  notify.proto
  speech.proto
  transmite.proto
  user.proto)

set(PROTO_SRCS)
set(PROTO_HDRS)
set(GRPC_SRCS)
set(GRPC_HDRS)

if(NOT CMAKE_BUILD_TYPE)
  message(FATAL_ERROR "CMAKE_BUILD_TYPE is required for Conan tool runtime paths")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" ZCHAT_CONAN_BUILD_TYPE)

set(ZCHAT_PROTOC_RUNTIME_DIRS)
foreach(PACKAGE_NAME protobuf abseil grpc re2 c-ares openssl zlib)
  set(LIB_DIRS_VAR "${PACKAGE_NAME}_LIB_DIRS_${ZCHAT_CONAN_BUILD_TYPE}")
  if(DEFINED ${LIB_DIRS_VAR})
    list(APPEND ZCHAT_PROTOC_RUNTIME_DIRS ${${LIB_DIRS_VAR}})
  endif()
endforeach()
list(REMOVE_DUPLICATES ZCHAT_PROTOC_RUNTIME_DIRS)

set(ZCHAT_PROTOC_ENV)
if(UNIX AND NOT APPLE)
  string(JOIN ":" ZCHAT_PROTOC_RUNTIME_PATH ${ZCHAT_PROTOC_RUNTIME_DIRS})
  list(APPEND ZCHAT_PROTOC_ENV
    "LD_LIBRARY_PATH=${ZCHAT_PROTOC_RUNTIME_PATH}:$ENV{LD_LIBRARY_PATH}")
elseif(APPLE)
  string(JOIN ":" ZCHAT_PROTOC_RUNTIME_PATH ${ZCHAT_PROTOC_RUNTIME_DIRS})
  list(APPEND ZCHAT_PROTOC_ENV
    "DYLD_LIBRARY_PATH=${ZCHAT_PROTOC_RUNTIME_PATH}:$ENV{DYLD_LIBRARY_PATH}")
endif()

foreach(PROTO_FILE ${PROTO_FILES})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.pb.cc")
  set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.pb.h")
  set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.cc")
  set(GRPC_HDR "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.h")
  add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND ${CMAKE_COMMAND} -E env ${ZCHAT_PROTOC_ENV}
      $<TARGET_FILE:protobuf::protoc>
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/generated
      --grpc_out=${CMAKE_CURRENT_BINARY_DIR}/generated
      --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
      -I ${CMAKE_CURRENT_SOURCE_DIR}
      --experimental_allow_proto3_optional
      ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}"
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
target_link_libraries(zchat_proto PUBLIC protobuf::protobuf gRPC::grpc++)
target_include_directories(zchat_proto
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)
