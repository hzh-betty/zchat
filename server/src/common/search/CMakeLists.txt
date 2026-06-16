add_library(zchat_search_module STATIC
  user_search_index.cc
  message_search_index.cc)
target_link_libraries(zchat_search_module
  PUBLIC
    zchat_common)