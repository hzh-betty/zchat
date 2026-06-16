add_library(zchat_common STATIC
  config.cc
  crypto.cc
  etcd_service.cc
  logger.cc
  notify_publisher.cc
  orm_helpers.cc
  proto_mapper.cc
  protobuf_http.cc
  runtime.cc
  session_store.cc
  uuid.cc)
target_link_libraries(zchat_common
  PUBLIC
    Drogon::Drogon
    OpenSSL::Crypto
    gRPC::grpc++
    spdlog::spdlog
    zchat_proto
    etcd-cpp-apiv3::etcd-cpp-apiv3)
target_include_directories(zchat_common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)
target_link_options(zchat_common
  PUBLIC
    -Wl,--copy-dt-needed-entries)

add_subdirectory(search)
