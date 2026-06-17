# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Single-source the app version (PROJECT_VER) from main/version.h so the
# ESP-IDF app description matches the OTA / SW-build version.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/main/version.h" _version_h)
string(REGEX MATCH "FW_VERSION_MAJOR[ \t]+([0-9]+)" _ "${_version_h}")
set(_v_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "FW_VERSION_MINOR[ \t]+([0-9]+)" _ "${_version_h}")
set(_v_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "FW_VERSION_PATCH[ \t]+([0-9]+)" _ "${_version_h}")
set(_v_patch "${CMAKE_MATCH_1}")
set(PROJECT_VER "${_v_major}.${_v_minor}.${_v_patch}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(light_switch)
