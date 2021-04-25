cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(white_star VERSION 0.1.0 LANGUAGES C CXX)

# === Includes ===

include(ExternalProject)
include(ProcessorCount)
include(CheckPIESupported)

# === Functions and macros ===

macro(dvar NAME)
  message(WARNING "${NAME}: ${${NAME}}")
endmacro()

function(set_cache_internal NAME)
  set(${NAME} ${ARGN} CACHE INTERNAL "")
endfunction()

function(set_target_property)
  cmake_parse_arguments(PARSE_ARGV 0 ARG "" "" PROPERTY)
  list(POP_FRONT ARG_PROPERTY PROPERTY_NAME)
  set_target_properties(${ARG_UNPARSED_ARGUMENTS} PROPERTIES ${PROPERTY_NAME} "${ARG_PROPERTY}")
endfunction()

# Get all targets added within the given directory tree.
function(get_targets RESULT_NAME DIR)
  get_directory_property(DIR_TARGETS DIRECTORY ${DIR} BUILDSYSTEM_TARGETS)
  get_directory_property(DIR_SUBDIRS DIRECTORY ${DIR} SUBDIRECTORIES)
  foreach(SUBDIR ${DIR_SUBDIRS})
    get_targets(SUBDIR_TARGETS ${SUBDIR})
    list(APPEND ALL_SUBDIR_TARGETS ${SUBDIR_TARGETS})
  endforeach()
  set(${RESULT_NAME} ${DIR_TARGETS} ${ALL_SUBDIR_TARGETS} PARENT_SCOPE)
endfunction()

# === Options ===

option(DEBUG_ENABLE_PCH "Enable precompiled headers in debug builds.")
option(DEBUG_ENABLE_UBSAN "Enable undefined behaviour sanitizer in debug builds.")
option(DEBUG_ENABLE_ASAN "Enable address sanitizer in debug builds.")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug or Release." FORCE)
endif()

# === Global settings ===

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_SKIP_RPATH OFF)
  set_cache_internal(BUILD_SHARED_LIBS ON)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_SKIP_RPATH ON)
  set_cache_internal(BUILD_SHARED_LIBS OFF)
endif()

set_cache_internal(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install_prefix)

ProcessorCount(NPROC)
if(NPROC EQUAL 0)
  set(NPROC 1)
endif()

# === Compiler and linker flags ===

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

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
check_pie_supported(LANGUAGES C CXX)

set(CXX_WARNING_FLAGS
  -Wall -Wextra -Wcast-qual -Wunused -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-declarations
  -Wmissing-include-dirs -Wredundant-decls -Wundef -Wfloat-equal -Wstack-protector -Wwrite-strings -Wvla -Wuninitialized
  -Wconversion -Wsign-conversion -Wpointer-arith -Wctor-dtor-privacy -Wnon-virtual-dtor -Woverloaded-virtual
  -Wsign-promo -Wold-style-cast -Wmisleading-indentation -Wnull-dereference -Wdouble-promotion
)

set(COMPILE_FLAGS
  -pipe -fstack-protector-strong -fno-plt -m64 -march=x86-64 -mtune=generic -pthread -D_XOPEN_SOURCE=700
)

if(CMAKE_GENERATOR STREQUAL "Ninja")
  list(APPEND COMPILE_FLAGS -fdiagnostics-color=always)
endif()

set(CXX_COMPILE_FLAGS)

set(LINK_FLAGS -Wl,-z,relro,-z,now,-z,defs,--sort-common)

set(PROJECT_COMPILE_FLAGS)
set(PROJECT_LINK_FLAGS)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND CXX_WARNING_FLAGS
    -Wcast-align=strict -Wlogical-op -Wstrict-null-sentinel -Wuseless-cast -Wduplicated-cond -Wduplicated-branches
  )
  list(APPEND LINK_FLAGS -fuse-ld=gold)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND CXX_WARNING_FLAGS -Wcast-align -Wimplicit-fallthrough -Wno-format-nonliteral)
  list(APPEND LINK_FLAGS -fuse-ld=lld)
endif()

set(COMPILE_FLAGS_DEFAULT_OPT
  -O3 -D_FORTIFY_SOURCE=2 -fno-strict-aliasing
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND COMPILE_FLAGS -ggdb1)
  list(APPEND LINK_FLAGS -Wl,--disable-new-dtags,--no-as-needed)

  list(APPEND PROJECT_COMPILE_FLAGS
    -DDEBUG -DHOT_RELOAD -D_GLIBCXX_DEBUG -D_FORTIFY_SOURCE=2 -Og -ggdb3 -fno-strict-aliasing
  )
  list(APPEND PROJECT_LINK_FLAGS -rdynamic)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND PROJECT_COMPILE_FLAGS -fvar-tracking -fvar-tracking-assignments)
  endif()

  if(DEBUG_ENABLE_UBSAN)
    list(APPEND PROJECT_COMPILE_FLAGS -fsanitize=undefined)
  endif()

  if(DEBUG_ENABLE_ASAN)
    list(APPEND PROJECT_COMPILE_FLAGS -fsanitize=address)
  endif()

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")

  list(APPEND COMPILE_FLAGS
    -fvisibility=hidden -g0 -DNDEBUG -fno-semantic-interposition -ffunction-sections -fdata-sections
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." ${COMPILE_FLAGS_DEFAULT_OPT}
  )

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND COMPILE_FLAGS -flto=${NPROC})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND COMPILE_FLAGS -flto=thin)
  endif()

  list(APPEND LINK_FLAGS -Wl,--as-needed,--gc-sections,-O1)
  list(APPEND CXX_COMPILE_FLAGS -fvisibility-inlines-hidden)

  list(APPEND CXX_WARNING_FLAGS -Wno-unused -Wno-unused-parameter)
endif()

add_compile_options(
  "$<$<COMPILE_LANGUAGE:CXX>:${CXX_COMPILE_FLAGS}>"
  ${COMPILE_FLAGS}
)
add_link_options(
  ${CXX_COMPILE_FLAGS}
  ${COMPILE_FLAGS}
  ${LINK_FLAGS}
)

get_directory_property(COMPLETE_COMPILE_FLAGS COMPILE_OPTIONS)
get_directory_property(COMPLETE_LINK_FLAGS LINK_OPTIONS)

# === Dependencies ===

add_library(glad deps/glad/src/glad.c)
target_compile_options(glad PRIVATE ${COMPILE_FLAGS_DEFAULT_OPT})
target_include_directories(glad PUBLIC deps/glad/include)
target_link_libraries(glad dl)

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE deps/glm)

add_library(earcut INTERFACE)
target_include_directories(earcut SYSTEM INTERFACE deps/earcut/include)

add_library(loguru deps/loguru/loguru.cpp)
target_compile_options(loguru PRIVATE ${COMPILE_FLAGS_DEFAULT_OPT})
target_include_directories(loguru PUBLIC deps/loguru)
target_compile_definitions(loguru PUBLIC LOGURU_USE_FMTLIB)
target_link_libraries(loguru fmt dl)

add_library(whereami deps/whereami/src/whereami.c)
target_compile_options(whereami PRIVATE ${COMPILE_FLAGS_DEFAULT_OPT})
target_include_directories(whereami PUBLIC deps/whereami/src)

set_cache_internal(GLFW_BUILD_DOCS OFF)
set_cache_internal(GLFW_BUILD_TESTS OFF)
set_cache_internal(GLFW_BUILD_EXAMPLES OFF)
set_cache_internal(GLFW_INSTALL OFF)
add_subdirectory(deps/glfw)
get_targets(GLFW_TARGETS deps/glfw)
set_target_property(${GLFW_TARGETS} PROPERTY COMPILE_OPTIONS "${COMPLETE_COMPILE_FLAGS}" ${COMPILE_FLAGS_DEFAULT_OPT})
set_target_property(${GLFW_TARGETS} PROPERTY LINK_OPTIONS
  "${COMPLETE_COMPILE_FLAGS}" ${COMPLETE_LINK_FLAGS} ${COMPILE_FLAGS_DEFAULT_OPT})

# --- fmt ---

add_subdirectory(deps/fmt)
get_targets(FMT_TARGETS deps/fmt)
set_target_property(${FMT_TARGETS} PROPERTY COMPILE_OPTIONS "${COMPLETE_COMPILE_FLAGS}" ${COMPILE_FLAGS_DEFAULT_OPT})
set_target_property(${FMT_TARGETS} PROPERTY LINK_OPTIONS
  "${COMPLETE_COMPILE_FLAGS}" ${COMPLETE_LINK_FLAGS} ${COMPILE_FLAGS_DEFAULT_OPT})

get_target_property(FMT_INTERFACE_LINK_LIBRARIES fmt INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM FMT_INTERFACE_LINK_LIBRARIES "-Wl,--as-needed")
set_target_property(fmt PROPERTY INTERFACE_LINK_LIBRARIES ${FMT_INTERFACE_LINK_LIBRARIES})

# --- SQLite ---

set(SQLITE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/deps/sqlite-amalgamation)

add_library(sqlite deps/sqlite-amalgamation/sqlite3.c)
target_compile_options(sqlite PRIVATE ${COMPILE_FLAGS_DEFAULT_OPT})
target_include_directories(sqlite PUBLIC ${SQLITE_INCLUDE_DIR})
target_compile_definitions(sqlite PUBLIC HAVE_USLEEP SQLITE_ENABLE_COLUMN_METADATA)
target_link_libraries(sqlite m dl)
set(SQLITE_LIBRARY_DIR ${CMAKE_BINARY_DIR})

add_executable(sqlite_cli deps/sqlite-amalgamation/shell.c)
target_compile_options(sqlite_cli PRIVATE ${COMPILE_FLAGS_DEFAULT_OPT})
target_link_libraries(sqlite_cli sqlite)

set_target_properties(sqlite sqlite_cli PROPERTIES OUTPUT_NAME sqlite)

# --- PROJ ---

set_cache_internal(BUILD_PROJSYNC OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set_cache_internal(BUILD_CCT OFF)
  set_cache_internal(BUILD_CS2CS OFF)
  set_cache_internal(BUILD_GEOD OFF)
  set_cache_internal(BUILD_GIE OFF)
  set_cache_internal(BUILD_PROJ OFF)
  set_cache_internal(BUILD_PROJINFO OFF)
endif()

set_cache_internal(EXE_SQLITE3 $<TARGET_FILE:sqlite_cli>)
set_cache_internal(SQLITE3_INCLUDE_DIR ${SQLITE_INCLUDE_DIR})
set_cache_internal(SQLITE3_LIBRARY $<TARGET_FILE:sqlite>)
set_cache_internal(BUILD_TESTING OFF)
set_cache_internal(ENABLE_CURL OFF)

# TODO: Build with libtiff
set_cache_internal(ENABLE_TIFF OFF)

add_subdirectory(deps/proj)
get_targets(PROJ_TARGETS deps/proj)
set_target_property(${PROJ_TARGETS} PROPERTY COMPILE_OPTIONS "${COMPLETE_COMPILE_FLAGS}" ${COMPILE_FLAGS_DEFAULT_OPT} -fno-lto)
set_target_property(${PROJ_TARGETS} PROPERTY LINK_OPTIONS
  "${COMPLETE_COMPILE_FLAGS}" ${COMPLETE_LINK_FLAGS} ${COMPILE_FLAGS_DEFAULT_OPT} -fno-lto)

set(PROJ_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/deps/proj/src ${CMAKE_SOURCE_DIR}/deps/proj/include)
target_include_directories(proj INTERFACE "$<BUILD_INTERFACE:${PROJ_INCLUDE_DIRS}>")

set(PROJ_LIBRARY_DIR ${CMAKE_BINARY_DIR}/deps/proj/lib)

# --- GDAL ---

set(EXTERNALPROJECT_LOG_ARGS
  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_INSTALL ON
  LOG_MERGED_STDOUTERR ON
  LOG_OUTPUT_ON_FAILURE ON
)

set(GDAL_SOURCE_DIR ${CMAKE_BINARY_DIR}/gdal_source)
set(GDAL_LIBRARY_DIR ${GDAL_SOURCE_DIR}/.libs)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(GDAL_RPATH ${SQLITE_LIBRARY_DIR} ${PROJ_LIBRARY_DIR} ${GDAL_LIBRARY_DIR})
  list(TRANSFORM GDAL_RPATH PREPEND -Wl,-rpath=)
endif()

set(GDAL_C_FLAGS ${COMPILE_FLAGS} ${COMPILE_FLAGS_DEFAULT_OPT} -fno-lto)
set(GDAL_CXX_FLAGS ${CXX_COMPILE_FLAGS} ${GDAL_C_FLAGS})
set(GDAL_LINK_FLAGS ${GDAL_CXX_FLAGS} ${LINK_FLAGS} ${GDAL_RPATH} -fno-lto)
list(JOIN GDAL_C_FLAGS " " GDAL_C_FLAGS_STRING)
list(JOIN GDAL_CXX_FLAGS " " GDAL_CXX_FLAGS_STRING)
list(JOIN GDAL_LINK_FLAGS " " GDAL_LINK_FLAGS_STRING)

set(INCLUDE_DIRS_OPTIONS ${PROJ_INCLUDE_DIRS} ${SQLITE_INCLUDE_DIR})
list(TRANSFORM INCLUDE_DIRS_OPTIONS PREPEND -I)
list(JOIN INCLUDE_DIRS_OPTIONS " " INCLUDE_DIRS_OPTIONS_STRING)

set(GDAL_ENV_VARS
  "CC=${CMAKE_C_COMPILER_LAUNCHER} ${CMAKE_C_COMPILER}"
  "CXX=${CMAKE_CXX_COMPILER_LAUNCHER} ${CMAKE_CXX_COMPILER}"
  "CFLAGS=${GDAL_C_FLAGS_STRING} ${INCLUDE_DIRS_OPTIONS_STRING}"
  "CXXFLAGS=${GDAL_CXX_FLAGS_STRING} ${INCLUDE_DIRS_OPTIONS_STRING}"
  "LDFLAGS=${GDAL_LINK_FLAGS_STRING} -L${SQLITE_LIBRARY_DIR} -L${PROJ_LIBRARY_DIR} -lm -ldl -lsqlite3"
)

set(GDAL_CONFIGURE_ARGS
  --prefix=${CMAKE_INSTALL_PREFIX}
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND GDAL_CONFIGURE_ARGS --enable-shared --disable-static --with-ld-shared --enable-rpath)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  list(APPEND GDAL_CONFIGURE_ARGS --disable-shared --enable-static --without-ld-shared --disable-rpath)
endif()

execute_process(COMMAND cp -alT ${CMAKE_SOURCE_DIR}/deps/gdal ${GDAL_SOURCE_DIR})

ExternalProject_Add(gdal_ep
  SOURCE_DIR ${GDAL_SOURCE_DIR}

  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${GDAL_ENV_VARS} ./configure ${GDAL_CONFIGURE_ARGS}

  BUILD_COMMAND ${CMAKE_COMMAND} -E env ${GDAL_ENV_VARS} make -j${NPROC}
  BUILD_IN_SOURCE ON

  INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${GDAL_ENV_VARS} make -j${NPROC} install

  EXCLUDE_FROM_ALL ON
  DEPENDS sqlite proj
  ${EXTERNALPROJECT_LOG_ARGS}
)

set(GDAL_INCLUDE_DIRS
  ${GDAL_SOURCE_DIR}/port
  ${GDAL_SOURCE_DIR}/gcore
  ${GDAL_SOURCE_DIR}/alg
  ${GDAL_SOURCE_DIR}/ogr
  ${GDAL_SOURCE_DIR}/ogr/ogrsf_frmts
  ${GDAL_SOURCE_DIR}/gnm
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(GDAL_LIBRARY_PATH ${GDAL_LIBRARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}gdal${CMAKE_SHARED_LIBRARY_SUFFIX})
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GDAL_LIBRARY_PATH ${GDAL_LIBRARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gdal${CMAKE_STATIC_LIBRARY_SUFFIX})
endif()

ExternalProject_Add_Step(gdal_ep post_build
  DEPENDEES build
  BYPRODUCTS ${GDAL_LIBRARY_PATH}
)

add_library(gdal INTERFACE)
target_include_directories(gdal SYSTEM INTERFACE ${GDAL_INCLUDE_DIRS})
target_link_libraries(gdal INTERFACE sqlite proj ${GDAL_LIBRARY_PATH})

add_custom_target(clean_gdal_ep
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/gdal_ep-prefix
  COMMAND make -C ${GDAL_SOURCE_DIR} -j${NPROC} -i clean
  VERBATIM
)

add_custom_target(clean_install
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_INSTALL_PREFIX} ${CMAKE_BINARY_DIR}/install_manifest.txt
  VERBATIM
)

# === This project ===

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_library(white_star_lib
    src/app.cpp
    src/render.cpp
  )

  add_executable(white_star src/main.cpp)

  if(DEBUG_ENABLE_PCH)
    target_precompile_headers(white_star_lib PRIVATE
      <glad/glad.h>
      <GLFW/glfw3.h>
      <glm/ext/matrix_clip_space.hpp>
      <glm/ext/matrix_transform.hpp>
      <glm/gtc/type_ptr.hpp>
      <glm/mat4x4.hpp>
      <glm/trigonometric.hpp>
      <glm/vec3.hpp>
      <loguru.hpp>
      <mapbox/earcut.hpp>
      <ogrsf_frmts.h>
      <whereami.h>

      <algorithm>
      <array>
      <cmath>
      <cstdint>
      <filesystem>
      <fstream>
      <functional>
      <initializer_list>
      <sstream>
      <string>
      <string.h>
      <utility>
    )
  endif()

  set(PROJECT_TARGETS white_star_lib white_star)

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_executable(white_star
    src/main.cpp
    src/app.cpp
    src/render.cpp
  )
  set(PROJECT_TARGETS white_star)
endif()

foreach(TARGET ${PROJECT_TARGETS})
  target_include_directories(${TARGET} PRIVATE src)
  target_compile_options(${TARGET} PRIVATE ${PROJECT_COMPILE_FLAGS} ${CXX_WARNING_FLAGS})
  target_link_options(${TARGET} PRIVATE ${PROJECT_COMPILE_FLAGS} ${PROJECT_LINK_FLAGS} ${CXX_WARNING_FLAGS})
  target_compile_definitions(${TARGET} PRIVATE GLFW_INCLUDE_NONE)
  target_link_libraries(${TARGET} glfw glad glm loguru fmt whereami gdal earcut)
endforeach()

file(CREATE_LINK ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/data SYMBOLIC)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/rebuild.sh ${CMAKE_BINARY_DIR}/rebuild.sh SYMBOLIC)
