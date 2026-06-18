add_executable(zchat_protocol_test protocol_test.cc)
target_link_libraries(zchat_protocol_test
  PRIVATE
    zchat_friend_module
    zchat_message_module
    zchat_common
    zchat_message_queue
    zchat_search_module)
add_test(NAME zchat_protocol_test COMMAND zchat_protocol_test)

add_executable(zchat_logger_test logger_test.cc)
target_link_libraries(zchat_logger_test PRIVATE zchat_common)
add_test(NAME zchat_logger_test COMMAND zchat_logger_test)

add_executable(zchat_crypto_test crypto_test.cc)
target_link_libraries(zchat_crypto_test PRIVATE zchat_common)
add_test(NAME zchat_crypto_test COMMAND zchat_crypto_test)

add_executable(zchat_uuid_test uuid_test.cc)
target_link_libraries(zchat_uuid_test PRIVATE zchat_common)
add_test(NAME zchat_uuid_test COMMAND zchat_uuid_test)

add_executable(zchat_password_test password_test.cc)
target_link_libraries(zchat_password_test PRIVATE zchat_common)
add_test(NAME zchat_password_test COMMAND zchat_password_test)