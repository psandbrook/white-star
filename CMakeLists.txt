cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(white-star VERSION 0.1.0 LANGUAGES C CXX)

include(ExternalProject)
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
set_cache_internal(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install_prefix)

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

set_cache_internal(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set_cache_internal(CMAKE_CXX_STANDARD 17)
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

get_directory_property(FULL_COMPILE_DEFINITIONS COMPILE_DEFINITIONS)
list(TRANSFORM FULL_COMPILE_DEFINITIONS PREPEND -D)

set(FULL_C_FLAGS ${FULL_COMPILE_DEFINITIONS} ${COMPILE_FLAGS})
set(FULL_CXX_FLAGS ${FULL_COMPILE_DEFINITIONS} ${CXX_COMPILE_FLAGS} ${COMPILE_FLAGS})
set(FULL_LINK_FLAGS ${CXX_COMPILE_FLAGS} ${COMPILE_FLAGS} ${LINK_FLAGS})
list(JOIN FULL_C_FLAGS " " FULL_C_FLAGS_STRING)
list(JOIN FULL_CXX_FLAGS " " FULL_CXX_FLAGS_STRING)
list(JOIN FULL_LINK_FLAGS " " FULL_LINK_FLAGS_STRING)

set(EXTERNALPROJECT_LOG_ARGS
  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_INSTALL ON
  LOG_MERGED_STDOUTERR ON
  LOG_OUTPUT_ON_FAILURE ON
)

add_library(glad deps/glad/src/glad.c)
target_include_directories(glad PUBLIC deps/glad/include)

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE deps/glm)

add_library(loguru deps/loguru/loguru.cpp)
target_include_directories(loguru PUBLIC deps/loguru)
target_compile_definitions(loguru PUBLIC LOGURU_USE_FMTLIB)
target_link_libraries(loguru fmt)

add_library(whereami deps/whereami/src/whereami.c)
target_include_directories(whereami PUBLIC deps/whereami/src)

set(SQLITE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/deps/sqlite-amalgamation)

add_library(sqlite deps/sqlite-amalgamation/sqlite3.c)
target_include_directories(sqlite PUBLIC ${SQLITE_INCLUDE_DIR})
target_compile_definitions(sqlite PUBLIC HAVE_USLEEP SQLITE_ENABLE_COLUMN_METADATA)
target_link_libraries(sqlite m dl)
set(SQLITE_LIBRARY_DIR ${CMAKE_BINARY_DIR})

add_executable(sqlite_cli deps/sqlite-amalgamation/shell.c)
target_link_libraries(sqlite_cli sqlite)

set_target_properties(sqlite sqlite_cli PROPERTIES OUTPUT_NAME sqlite3)

set_cache_internal(GLFW_BUILD_DOCS OFF)
set_cache_internal(GLFW_BUILD_TESTS OFF)
set_cache_internal(GLFW_BUILD_EXAMPLES OFF)
set_cache_internal(GLFW_INSTALL OFF)
add_subdirectory(deps/glfw)

add_subdirectory(deps/fmt)

set_cache_internal(BUILD_PROJSYNC OFF)
set_cache_internal(EXE_SQLITE3 $<TARGET_FILE:sqlite_cli>)
set_cache_internal(SQLITE3_INCLUDE_DIR ${SQLITE_INCLUDE_DIR})
set_cache_internal(SQLITE3_LIBRARY $<TARGET_FILE:sqlite>)
set_cache_internal(BUILD_TESTING OFF)
set_cache_internal(ENABLE_CURL OFF)
set_cache_internal(ENABLE_TIFF OFF)
add_subdirectory(deps/proj)

set(PROJ_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/deps/proj/src ${CMAKE_SOURCE_DIR}/deps/proj/include)
target_include_directories(proj INTERFACE "$<BUILD_INTERFACE:${PROJ_INCLUDE_DIRS}>")

set(PROJ_LIBRARY_DIR ${CMAKE_BINARY_DIR}/deps/proj/lib)

set(PROJ_INCLUDE_DIRS_OPTIONS ${PROJ_INCLUDE_DIRS})
list(TRANSFORM PROJ_INCLUDE_DIRS_OPTIONS PREPEND -I)
list(JOIN PROJ_INCLUDE_DIRS_OPTIONS " " PROJ_INCLUDE_DIRS_OPTIONS_STRING)

set(GDAL_ENV_VARS
  "CC=${CMAKE_C_COMPILER_LAUNCHER} ${CMAKE_C_COMPILER}"
  "CXX=${CMAKE_CXX_COMPILER_LAUNCHER} ${CMAKE_CXX_COMPILER}"
  "CFLAGS=${FULL_C_FLAGS_STRING} ${PROJ_INCLUDE_DIRS_OPTIONS_STRING}"
  "CXXFLAGS=${FULL_CXX_FLAGS_STRING} ${PROJ_INCLUDE_DIRS_OPTIONS_STRING}"
  "LDFLAGS=${FULL_LINK_FLAGS_STRING} -L${SQLITE_LIBRARY_DIR} -L${PROJ_LIBRARY_DIR} -lm -ldl -lsqlite3"
)

set(GDAL_CONFIGURE_ARGS
  --prefix=${CMAKE_INSTALL_PREFIX}
  --disable-shared
  --without-ld-shared
  --enable-static
  --disable-rpath
  --with-cpp14
  --with-pic
  --with-hide-internal-symbols
  --with-threads

  --disable-all-optional-drivers
  --disable-pdf-plugin

  --enable-driver-gpkg

  --with-libtiff=internal
  --with-geotiff=internal
  --with-libjson-c=internal

  --with-proj
  --with-sqlite3

  --without-libz
  --without-liblzma
  --without-zstd
  --without-pg
  --without-grass
  --without-libgrass
  --without-cfitsio
  --without-pcraster
  --without-png
  --without-dds
  --without-gta
  --without-pcidsk
  --without-jpeg
  --without-charls
  --without-jpeg12
  --without-gif
  --without-ogdi
  --without-fme
  --without-sosi
  --without-mongocxx
  --without-boost-lib-path
  --without-mongocxxv3
  --without-hdf4
  --without-hdf5
  --without-kea
  --without-netcdf
  --without-jasper
  --without-openjpeg
  --without-fgdb
  --without-ecw
  --without-kakadu
  --without-mrsid
  --without-jp2mrsid
  --without-mrsid_lidar
  --without-jp2lura
  --without-msg
  --without-oci
  --without-gnm
  --without-mysql
  --without-ingres
  --without-xerces
  --without-expat
  --without-libkml
  --without-odbc
  --without-dods-root
  --without-curl
  --without-xml2
  --without-spatialite
  --without-rasterlite2
  --without-pcre
  --without-teigha
  --without-teigha-plt
  --without-idb
  --without-sde
  --without-epsilon
  --without-webp
  --without-geos
  --without-sfcgal
  --without-qhull
  --without-opencl
  --without-freexl
  --without-pam
  --without-poppler
  --without-podofo
  --without-pdfium
  --without-perl
  --without-python
  --without-java
  --without-hdfs
  --without-tiledb
  --without-mdb
  --without-jvm-lib
  --without-jvm-lib-add-rpath
  --without-rasdaman
  --without-rdb
  --without-armadillo
  --without-cryptopp
  --without-crypto
  --without-lerc
  --without-null
  --without-exr
)

ExternalProject_Add(gdal_ep
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/gdal

  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${GDAL_ENV_VARS} ./configure ${GDAL_CONFIGURE_ARGS}

  BUILD_COMMAND ${CMAKE_COMMAND} -E env ${GDAL_ENV_VARS} make -j${NPROC}
  BUILD_IN_SOURCE ON

  INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${GDAL_ENV_VARS} make -j${NPROC} install

  DEPENDS sqlite proj
  ${EXTERNALPROJECT_LOG_ARGS}
)

set(GDAL_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/deps/gdal/port
  ${CMAKE_SOURCE_DIR}/deps/gdal/gcore
  ${CMAKE_SOURCE_DIR}/deps/gdal/alg
  ${CMAKE_SOURCE_DIR}/deps/gdal/ogr
  ${CMAKE_SOURCE_DIR}/deps/gdal/ogr/ogrsf_frmts
  ${CMAKE_SOURCE_DIR}/deps/gdal/gnm
)
set(GDAL_LIBRARY_PATH ${CMAKE_SOURCE_DIR}/deps/gdal/.libs/${CMAKE_STATIC_LIBRARY_PREFIX}gdal${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add_Step(gdal_ep post-build
  DEPENDEES build
  BYPRODUCTS ${GDAL_LIBRARY_PATH}
)

add_library(gdal INTERFACE)
target_include_directories(gdal INTERFACE ${GDAL_INCLUDE_DIRS})
target_link_libraries(gdal INTERFACE sqlite proj ${GDAL_LIBRARY_PATH})

add_custom_target(clean-gdal_ep
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/gdal_ep-prefix
  COMMAND make -C ${CMAKE_SOURCE_DIR}/deps/gdal -j${NPROC} -i clean
  VERBATIM
)

add_custom_target(clean-install
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_INSTALL_PREFIX} ${CMAKE_BINARY_DIR}/install_manifest.txt
  VERBATIM
)

add_executable(white-star ${SOURCES})
target_include_directories(white-star PRIVATE src)
target_compile_options(white-star PRIVATE ${LOCAL_COMPILE_FLAGS} ${CXX_WARNING_FLAGS})
target_link_options(white-star PRIVATE ${LOCAL_COMPILE_FLAGS} ${CXX_WARNING_FLAGS})
target_compile_definitions(white-star PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(white-star glfw glad glm loguru fmt whereami gdal)

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
