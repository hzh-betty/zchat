add_library(zchat_message_module STATIC
  message_search_index.cc
  message_service.cc
  orm_message_repository.cc)
target_link_libraries(zchat_message_module PUBLIC zchat_common)

add_executable(zchat_message_service
  message_builder.cc
  message_context.cc
  message_grpc_service.cc
  message_main.cc)
target_link_libraries(zchat_message_service
  PRIVATE
    zchat_file_module
    zchat_friend_module
    zchat_message_module
    zchat_message_queue
    zchat_user_module)
