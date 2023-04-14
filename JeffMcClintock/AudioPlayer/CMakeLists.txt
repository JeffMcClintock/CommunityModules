cmake_minimum_required(VERSION 3.15)

# fix warning on macOS about importing choc library
cmake_policy(SET CMP0114 NEW)

include(ExternalProject)

project(AudioPlayer)

# Add choc library, for MP3 playback
# Since it's a header-only library, we don't run cmake on it. hence the empty CONFIGURE_COMMAND and BUILD_COMMAND
ExternalProject_Add(
  choc
  GIT_REPOSITORY https://github.com/Tracktion/choc
  GIT_TAG origin/main
  PREFIX choc
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

build_gmpi_plugin(
    PROJECT_NAME ${PROJECT_NAME}
    HAS_DSP
    SOURCE_FILES 
        ${PROJECT_NAME}.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/choc/src/choc)
