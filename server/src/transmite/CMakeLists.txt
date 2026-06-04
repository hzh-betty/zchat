add_library(zchat_message_queue STATIC message_queue.cc)
target_link_libraries(zchat_message_queue
  PUBLIC
    zchat_common
    amqpcpp
    libevent::core
    libevent::pthreads)

add_library(zchat_transmite_module STATIC
  transmite_service.cc
  orm_transmite_repository.cc)
target_link_libraries(zchat_transmite_module
  PUBLIC
    zchat_message_module
    zchat_message_queue
    zchat_repository
    zchat_user_module)

add_executable(zchat_transmite_service
  transmite_builder.cc
  transmite_context.cc
  transmite_grpc_service.cc
  transmite_main.cc)
target_link_libraries(zchat_transmite_service PRIVATE zchat_transmite_module)
