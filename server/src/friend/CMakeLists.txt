add_library(zchat_friend_module STATIC
  friend_service.cc
  orm_friend_repository.cc)
target_link_libraries(zchat_friend_module PUBLIC zchat_repository)

add_executable(zchat_friend_service
  friend_builder.cc
  friend_context.cc
  friend_grpc_service.cc
  friend_main.cc)
target_link_libraries(zchat_friend_service
  PRIVATE
    zchat_file_module
    zchat_friend_module
    zchat_message_module
    zchat_user_module)
