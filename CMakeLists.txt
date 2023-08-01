cmake_minimum_required(VERSION 3.10)
project(mrb VERSION 1.0)

include(ExternalProject)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0072 NEW)

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/external/mruby/Rakefile)
    message(FATAL_ERROR "mruby submodule not found. Have you done 'git submodule update --init' ?")
endif()

if(EXISTS /opt/vc)
    set(RASPBERRY_PI 1)
endif()

# System wide directories and flags

if(RASPBERRY_PI)
    set(VC /opt/vc)
    include_directories(SYSTEM
        ${VC}/include
        ${VC}/include/interface/vcos/pthreads
        ${VC}/include/interface/vmcs_host/linux)
    link_directories(${VC}/lib)
    add_compile_definitions(RASPBERRY_PI)
endif()

if(WIN32)
  add_compile_options(/utf-8)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_USE_MATH_DEFINES
                  /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

if(APPLE)
    include_directories(SYSTEM /usr/local/include /opt/homebrew/include)
    link_directories(/usr/local/lib /opt/homebrew/lib)
    #add_compile_options(-target x86_64-apple-macos10.15)
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

add_subdirectory(external/fmt)
#add_subdirectory(external/CLI11)
#add_subdirectory(external/coreutils)
#add_subdirectory(external/glm)
add_subdirectory(external/doctest)
#add_subdirectory(external/flite)

set(MRB ${PROJECT_SOURCE_DIR}/external/mruby)
set(MRB_LIB ${PROJECT_SOURCE_DIR}/builds/mruby/lib)
set(MRB_INC ${MRB}/include)

if(RASPBERRY_PI)
    add_library(mruby INTERFACE)
    target_link_libraries(mruby INTERFACE ${MRB_LIB}/libmruby.a
        ${MRB_LIB}/libmruby_core.a)
    target_include_directories(mruby INTERFACE ${MRB}/include)
    target_compile_definitions(mruby INTERFACE MRB_INT32 MRB_32BIT)
elseif(EMSCRIPTEN)
    add_library(mruby INTERFACE)
    set(MRB_LIB ${PROJECT_SOURCE_DIR}/builds/embuild/lib)
    target_link_libraries(mruby INTERFACE ${MRB_LIB}/libmruby.a
        ${MRB_LIB}/libmruby_core.a)
    target_include_directories(mruby INTERFACE ${MRB}/include)
    target_compile_definitions(mruby INTERFACE MRB_INT32 MRB_32BIT)
elseif(WIN32)
    set(EXTRA_LIBRARIES ws2_32)
    set(WINDIR ${PROJECT_SOURCE_DIR}/win)
    add_library(mruby INTERFACE)
    target_link_libraries(mruby INTERFACE ${WINDIR}/mruby/libmruby.lib
        ${WINDIR}/mruby/libmruby_core.lib)
    target_include_directories(mruby INTERFACE ${MRB}/include)
    target_compile_definitions(mruby INTERFACE MRB_INT32 MRB_32BIT)
else()
    add_library(mruby INTERFACE)
    target_link_libraries(mruby INTERFACE ${MRB_LIB}/libmruby.a
        ${MRB_LIB}/libmruby_core.a)
    target_include_directories(mruby INTERFACE ${MRB}/include)
    target_compile_definitions(mruby INTERFACE MRB_INT32 MRB_32BIT)
endif()

find_package(Threads REQUIRED)

add_library(_mrb INTERFACE)
target_include_directories(_mrb INTERFACE src)
target_link_libraries(_mrb INTERFACE mruby)
add_library(mrb::mrb ALIAS _mrb)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_executable(mrbtest tests/testmain.cpp
        tests/mrb_conv_test.cpp tests/mrb_args_test.cpp)
    target_include_directories(mrbtest PRIVATE src)
    target_link_libraries(mrbtest PRIVATE mrb_Warnings mrb::mrb fmt::fmt mruby doctest)
endif()

