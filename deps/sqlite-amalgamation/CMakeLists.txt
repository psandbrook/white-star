cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(sqlite LANGUAGES C)

option(SQLITE_INSTALL "Include install commands for sqlite" ON)

unset(ENV{CPPFLAGS})
unset(ENV{CFLAGS})
unset(ENV{CXXFLAGS})
unset(ENV{LDFLAGS})

add_compile_definitions(SQLITE_ENABLE_COLUMN_METADATA)

add_library(sqlite3 sqlite3.c)
target_include_directories(sqlite3 PUBLIC .)
target_compile_definitions(sqlite3 PRIVATE HAVE_USLEEP)
target_compile_options(sqlite3 PUBLIC -pthread)
target_link_options(sqlite3 PUBLIC -pthread)
target_link_libraries(sqlite3 dl)
set_property(TARGET sqlite3 PROPERTY PUBLIC_HEADER sqlite3.h sqlite3ext.h)

add_executable(sqlite3_cli shell.c)
target_link_libraries(sqlite3_cli sqlite3)
set_target_properties(sqlite3_cli PROPERTIES OUTPUT_NAME sqlite3)

if(SQLITE_INSTALL)
  install(TARGETS sqlite3 sqlite3_cli)
endif()
