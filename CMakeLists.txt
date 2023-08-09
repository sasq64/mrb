cmake_minimum_required(VERSION 3.10)
project(mrb VERSION 1.0)

include(ExternalProject)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0072 NEW)

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/external/mruby/Rakefile)
    message(FATAL_ERROR "mruby submodule not found. Have you done 'git submodule update --init' ?")
endif()

# System wide directories and flags

if(WIN32)
  add_compile_options(/utf-8)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_USE_MATH_DEFINES
                  /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

if(APPLE)
    include_directories(SYSTEM /usr/local/include /opt/homebrew/include)
    link_directories(/usr/local/lib /opt/homebrew/lib)
endif()

if(CMAKE_CXX_COMPILER MATCHES "/em\\+\\+(-[a-zA-Z0-9.])?$")
  message(" * Emscripten")
  set(CMAKE_CXX_COMPILER_ID "Emscripten")
  set(EMSCRIPTEN 1)
else()
  message(" * ${CMAKE_CXX_COMPILER_ID}")
endif()



# Warning level setup #######################################################

# GCC/Clang common warnings
set(GCC_CLANG_WARNINGS
    -Werror
    -Wall
    -Wextra
    -Wnon-virtual-dtor
    -pedantic
    -Wno-unused-variable
    -Wno-unused-parameter
    -Wno-unused-function)

set(MSVC_WARNINGS /W2  /wd4244)

# GCC specific warnings
set(GCC_WARNINGS -Wno-duplicated-branches -Wduplicated-cond -Wlogical-op
                 -Wcast-align -Wno-missing-field-initializers)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(WARNINGS ${WARNINGS} ${GCC_WARNINGS} ${GCC_CLANG_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(WARNINGS ${WARNINGS} ${CLANG_WARNINGS} ${GCC_CLANG_WARNINGS})
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(WARNINGS ${WARNINGS} ${MSVC_WARNINGS})
endif()

add_library(mrb_Warnings INTERFACE)
target_compile_options(mrb_Warnings INTERFACE ${WARNINGS})
add_subdirectory(external/doctest)

set(MRB ${PROJECT_SOURCE_DIR}/external/mruby)
set(MRB_LIB ${PROJECT_SOURCE_DIR}/builds/mruby/lib)
set(MRB_INC ${MRB}/include)
set(MRB_CONF mruby.cfg)

if(EMSCRIPTEN)
    set(MRB_LIB ${PROJECT_SOURCE_DIR}/builds/embuild/lib)
    set(MRB_CONF emruby.cfg)
endif()

add_library(mruby INTERFACE)
if(WIN32)
    set(EXTRA_LIBRARIES ws2_32)
    set(WINDIR ${PROJECT_SOURCE_DIR}/win)
    target_link_libraries(mruby INTERFACE ${WINDIR}/mruby/libmruby.lib
        ${WINDIR}/mruby/libmruby_core.lib)
else()
    add_custom_target(mruby_rake ALL
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND MRUBY_CONFIG=${MRB_CONF} rake -f external/mruby/Rakefile -j8 -v)

    add_dependencies(mruby murby_rake)

    target_link_libraries(mruby INTERFACE ${MRB_LIB}/libmruby.a
        ${MRB_LIB}/libmruby_core.a)
endif()

target_include_directories(mruby INTERFACE ${MRB}/include)
target_compile_definitions(mruby INTERFACE MRB_INT32 MRB_32BIT)

find_package(Threads REQUIRED)

add_library(_mrb INTERFACE
    src/mrb/value.hpp)
target_include_directories(_mrb INTERFACE src)
target_link_libraries(_mrb INTERFACE mruby mrb_Warnings)
add_library(mrb::mrb ALIAS _mrb)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_executable(mrbtest tests/testmain.cpp
        tests/mrb_conv_test.cpp tests/mrb_args_test.cpp)
    target_include_directories(mrbtest PRIVATE src)
    target_link_libraries(mrbtest PRIVATE mrb_Warnings mrb::mrb mruby doctest)
endif()

