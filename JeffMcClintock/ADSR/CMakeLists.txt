cmake_minimum_required(VERSION 3.15)
project(ADSR)
set(sdk_folder
../../se_sdk3
)
set(sdk_srcs
${sdk_folder}/mp_sdk_common.cpp
${sdk_folder}/mp_sdk_audio.cpp
)
set(srcs
Adsr.rc
Adsr.cpp
)
include_directories(
${sdk_folder}
)
include (GenerateExportHeader)
add_library(${PROJECT_NAME} SHARED ${sdk_srcs} ${srcs})
set_property(TARGET ${PROJECT_NAME} PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
