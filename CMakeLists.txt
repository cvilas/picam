
cmake_minimum_required(VERSION 3.31.6)
project(grape LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera>=0.5)

set(SOURCES image_frame.h camera.h camera.cpp main.cpp)
add_executable(picam ${SOURCES})
target_link_libraries(picam PkgConfig::LIBCAMERA)

