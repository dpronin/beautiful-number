cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(task VERSION 1.1.1 LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(${PROJECT_NAME}
    task.cpp
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -march=native
    -Wall
    -Wextra
    -Werror
    $<$<NOT:$<STREQUAL:$<LOWER_CASE:${CMAKE_CXX_COMPILER_ID}>,clang>>:-Wno-interference-size>
    $<$<CONFIG:Debug>:-ggdb3>
)

if (${CMAKE_BUILD_TYPE} MATCHES "Release" OR ${CMAKE_BUILD_TYPE} MATCHES "RelWithDebInfo")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result OUTPUT output)
  if (result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else ()
    message(WARNING "IPO is not supported: ${output}")
  endif ()
  set(CMAKE_C_VISIBILITY_PRESET hidden)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
