cmake_minimum_required(VERSION 3.26.3)
project(Incogine)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2.framework")
find_package(SDL2 REQUIRED COMPONENTS SDL2)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB MY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(Incogine ${MY_SOURCES})
target_link_libraries(Incogine PRIVATE SDL2::SDL2)