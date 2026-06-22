add_library(zchat_message_module STATIC
  message_service.cc
  orm_message_repository.cc)
target_link_libraries(zchat_message_module
  PUBLIC
    zchat_common
    zchat_search_module)

add_executable(zchat_message_service
  message_context.cc
  message_grpc_service.cc
  message_main.cc)
target_link_libraries(zchat_message_service
  PRIVATE
    zchat_message_module
    zchat_message_queue)