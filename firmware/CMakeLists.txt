# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Apply project defaults when generating sdkconfig.
# (Without this, a developer's existing build directory may keep an older
# sdkconfig where TinyUSB MIDI is disabled.)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(emiuet_firmware)
