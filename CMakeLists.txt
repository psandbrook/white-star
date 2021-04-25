cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(white-star VERSION 0.1.0 LANGUAGES C CXX)

include(ProcessorCount)

function(set_cache_internal NAME)
  set(${NAME} ${ARGN} CACHE INTERNAL "")
endfunction()

ProcessorCount(NPROC)
if(NPROC EQUAL 0)
  set(NPROC 1)
endif()

option(DEBUG_ENABLE_UBSAN "Enable undefined behaviour sanitizer in debug builds.")
option(DEBUG_ENABLE_ASAN "Enable address sanitizer in debug builds.")
option(DEBUG_ENABLE_PCH "Enable precompiled headers in debug builds.")

# This is off by default to reduce compile times.
option(DEBUG_ENABLE_DEBUG_INFO "Enable debug information in debug builds.")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug or Release." FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SKIP_RPATH ON)

set_cache_internal(BUILD_SHARED_LIBS OFF)
set_cache_internal(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install_prefix")

set(SOURCES
  src/main.cpp
  src/app_state.cpp
  src/render.cpp
  src/filesystem.cpp
)

unset(ENV{CPPFLAGS})
unset(ENV{CFLAGS})
unset(ENV{CXXFLAGS})
unset(ENV{LDFLAGS})
set(CMAKE_C_FLAGS "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CXX_WARNING_FLAGS
  -Wall -Wextra -Wcast-qual -Wunused -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-declarations
  -Wmissing-include-dirs -Wredundant-decls -Wundef -Wfloat-equal -Wstack-protector -Wwrite-strings -Wvla -Wuninitialized
  -Wconversion -Wsign-conversion -Wpointer-arith -Wctor-dtor-privacy -Wnon-virtual-dtor -Woverloaded-virtual
  -Wsign-promo -Wold-style-cast -Wmisleading-indentation -Wnull-dereference -Wdouble-promotion
)

set(COMPILE_FLAGS
  -pipe -fvisibility=hidden -fstack-protector-strong -fno-plt -m64 -march=native -fPIE -pthread
)

set(LOCAL_COMPILE_FLAGS)
set(LINK_FLAGS -pie -Wl,-z,relro,-z,now,-z,defs,--sort-common,--as-needed)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND CXX_WARNING_FLAGS
    -Wcast-align=strict -Wlogical-op -Wstrict-null-sentinel -Wuseless-cast -Wduplicated-cond -Wduplicated-branches
  )
  list(APPEND LINK_FLAGS -fuse-ld=gold)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND CXX_WARNING_FLAGS -Wcast-align -Wimplicit-fallthrough -Wno-format-nonliteral)
  list(APPEND LINK_FLAGS -fuse-ld=lld)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND COMPILE_FLAGS -DDEBUG)

  if(DEBUG_ENABLE_DEBUG_INFO)
    list(APPEND COMPILE_FLAGS -Og -g)
    list(APPEND LINK_FLAGS -rdynamic)
  else()
    list(APPEND COMPILE_FLAGS -O0 -g0)
  endif()

  if(DEBUG_ENABLE_UBSAN)
    list(APPEND LOCAL_COMPILE_FLAGS -fsanitize=undefined)
  endif()

  if(DEBUG_ENABLE_ASAN)
    list(APPEND LOCAL_COMPILE_FLAGS -fsanitize=address)
  endif()

  add_compile_definitions(_GLIBCXX_DEBUG)

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  list(APPEND CXX_WARNING_FLAGS -Wno-unused -Wno-unused-parameter)

  list(APPEND COMPILE_FLAGS
    -O3 -g0 -DNDEBUG -D_FORTIFY_SOURCE=2 -fno-semantic-interposition -ffunction-sections -fdata-sections
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
  )

  list(APPEND LINK_FLAGS -Wl,--gc-sections,-O1)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND COMPILE_FLAGS -flto=${NPROC})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND COMPILE_FLAGS -flto=thin)
  endif()
endif()

list(APPEND COMPILE_FLAGS -fno-strict-aliasing)

add_compile_definitions(_XOPEN_SOURCE=700)

if(CMAKE_GENERATOR STREQUAL "Ninja")
  list(APPEND COMPILE_FLAGS -fdiagnostics-color=always)
endif()

set(CXX_COMPILE_FLAGS -fvisibility-inlines-hidden)

add_compile_options(
  "$<$<COMPILE_LANGUAGE:CXX>:${CXX_COMPILE_FLAGS}>"
  ${COMPILE_FLAGS}
)
add_link_options(
  ${CXX_COMPILE_FLAGS}
  ${COMPILE_FLAGS}
  ${LINK_FLAGS}
)

set_cache_internal(GLFW_BUILD_DOCS OFF)
set_cache_internal(GLFW_BUILD_TESTS OFF)
set_cache_internal(GLFW_BUILD_EXAMPLES OFF)
add_subdirectory(deps/glfw)

add_library(glad deps/glad/src/glad.c)
target_include_directories(glad PUBLIC deps/glad/include)

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE deps/glm)

add_subdirectory(deps/fmt)

add_library(loguru deps/loguru/loguru.cpp)
target_include_directories(loguru PUBLIC deps/loguru)
target_compile_definitions(loguru PUBLIC LOGURU_USE_FMTLIB)
target_link_libraries(loguru fmt)

add_library(whereami deps/whereami/src/whereami.c)
target_include_directories(whereami PUBLIC deps/whereami/src)

add_executable(white-star ${SOURCES})
target_include_directories(white-star PRIVATE src)
target_compile_options(white-star PRIVATE ${LOCAL_COMPILE_FLAGS} ${CXX_WARNING_FLAGS})
target_link_options(white-star PRIVATE ${LOCAL_COMPILE_FLAGS} ${CXX_WARNING_FLAGS})
target_compile_definitions(white-star PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(white-star glfw glad glm loguru fmt whereami)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND DEBUG_ENABLE_PCH)
  target_precompile_headers(white-star PUBLIC
    <GLFW/glfw3.h>
    <glad/glad.h>
    <loguru.hpp>

    <algorithm>
    <array>
    <cmath>
    <cstdint>
    <functional>
    <utility>
  )
endif()

file(CREATE_LINK ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/data SYMBOLIC)
