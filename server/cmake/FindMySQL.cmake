# 这是一个为了修复 Drogon 的 FindMySQL 无法在 Linux 下找到无 _r 后缀的 mysqlclient 库的补丁模块。
# 它作为 CMAKE_MODULE_PATH 的一部分被 Drogon 内部的 find_dependency(MySQL) 调用。

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_MYSQLCLIENT QUIET mysqlclient)
endif()

if(PC_MYSQLCLIENT_FOUND)
  set(MYSQL_INCLUDE_DIRS "${PC_MYSQLCLIENT_INCLUDE_DIRS}")
  set(MYSQL_LIBRARIES "${PC_MYSQLCLIENT_LINK_LIBRARIES}")
else()
  find_path(MYSQL_INCLUDE_DIRS NAMES mysql.h PATH_SUFFIXES mysql mariadb)
  # 修复了 Drogon 漏掉查找 mysqlclient 库名的 Bug（原有脚本只找了 mysqlclient_r mariadbclient）
  find_library(MYSQL_LIBRARIES NAMES mysqlclient mysqlclient_r mariadbclient)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL REQUIRED_VARS MYSQL_INCLUDE_DIRS MYSQL_LIBRARIES)

if(MySQL_FOUND AND NOT TARGET MySQL_lib)
  add_library(MySQL_lib INTERFACE IMPORTED)
  if(PC_MYSQLCLIENT_FOUND)
    target_link_libraries(MySQL_lib INTERFACE PkgConfig::PC_MYSQLCLIENT)
  else()
    target_include_directories(MySQL_lib INTERFACE "${MYSQL_INCLUDE_DIRS}")
    target_link_libraries(MySQL_lib INTERFACE "${MYSQL_LIBRARIES}")
  endif()
endif()
