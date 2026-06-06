add_library(zchat_file_module STATIC
  file_service.cc
  orm_file_repository.cc)
target_link_libraries(zchat_file_module PUBLIC zchat_common)

add_executable(zchat_file_service
  file_builder.cc
  file_context.cc
  file_grpc_service.cc
  file_main.cc)
target_link_libraries(zchat_file_service
  PRIVATE
    zchat_file_module)
