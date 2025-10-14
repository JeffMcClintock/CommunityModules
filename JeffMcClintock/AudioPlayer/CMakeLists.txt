cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)

# fix warning on macOS about importing choc library
cmake_policy(SET CMP0114 NEW)

include(FetchContent)

project(AudioPlayer)

FetchContent_Declare(
  choc
  GIT_REPOSITORY https://github.com/Tracktion/choc
  GIT_TAG origin/main
)

FetchContent_MakeAvailable(choc)
include_directories(${choc_SOURCE_DIR}/choc)

gmpi_plugin(
    PROJECT_NAME ${PROJECT_NAME}
    HAS_DSP
    SOURCE_FILES 
        ${PROJECT_NAME}.cpp
)

