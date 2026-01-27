
cmake_minimum_required(VERSION 3.31.6)
project(grape LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_SOURCE_DIR}/cmake/formatter.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/linter.cmake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera>=0.5)

find_package(OpenGL REQUIRED)
pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3>=3.3)

set(SOURCES 
  image_frame.h 
  camera.h 
  camera.cpp 
  display.h 
  display.cpp 
  main.cpp
)

add_executable(picam ${SOURCES})
target_link_libraries(picam PkgConfig::LIBCAMERA PkgConfig::GLFW3 OpenGL::GL)
add_clang_format(picam)

