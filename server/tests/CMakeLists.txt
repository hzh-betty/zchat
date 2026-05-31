add_executable(zchat_protocol_test protocol_test.cc)
target_link_libraries(zchat_protocol_test
  PRIVATE
    zchat_file_module
    zchat_friend_module
    zchat_message_module
    zchat_message_queue
    zchat_transmite_module
    zchat_user_module)
add_test(NAME zchat_protocol_test COMMAND zchat_protocol_test)

add_executable(zchat_logger_test logger_test.cc)
target_link_libraries(zchat_logger_test PRIVATE zchat_common)
add_test(NAME zchat_logger_test COMMAND zchat_logger_test)
