add_library(zchat_friend_module STATIC
  friend_service.cc
  orm_friend_repository.cc)
target_link_libraries(zchat_friend_module
  PUBLIC
    zchat_common
    zchat_search_module)

add_executable(zchat_friend_service
  friend_builder.cc
  friend_context.cc
  friend_grpc_service.cc
  friend_main.cc)
target_link_libraries(zchat_friend_service
  PRIVATE
    zchat_friend_module)