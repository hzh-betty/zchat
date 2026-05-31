add_library(zchat_user_module STATIC
  alibaba_sms_client.cc
  orm_user_repository.cc
  sms_client.cc
  user_search_index.cc
  user_service.cc)
target_link_libraries(zchat_user_module PUBLIC zchat_repository)

add_executable(zchat_user_service
  user_builder.cc
  user_context.cc
  user_grpc_service.cc
  user_main.cc)
target_link_libraries(zchat_user_service
  PRIVATE
    zchat_file_module
    zchat_user_module)
