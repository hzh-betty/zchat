add_library(zchat_speech_module STATIC
  baidu_speech_recognizer.cc
  speech_service.cc
  speech_recognizer.cc)
target_link_libraries(zchat_speech_module PUBLIC zchat_common)

add_executable(zchat_speech_service
  speech_builder.cc
  speech_context.cc
  speech_grpc_service.cc
  speech_main.cc)
target_link_libraries(zchat_speech_service
  PRIVATE
    zchat_speech_module
    gRPC::grpc++
    grpc++
    grpc
    gpr)
