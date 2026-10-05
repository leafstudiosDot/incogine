cmake_minimum_required(VERSION 3.21)

# This adapter lets Gradle drive the engine's own CMakeLists.txt (which
# builds SDL3 + addons from the reqs/ submodules and compiles the game into
# the "main" shared library that SDLActivity loads).
#
# The engine repository root sits three directories up from this file:
#   <repo>/android-project/app/jni/CMakeLists.txt  ->  <repo>/CMakeLists.txt
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. engine)
