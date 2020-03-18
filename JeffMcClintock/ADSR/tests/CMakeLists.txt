cmake_minimum_required(VERSION 3.15)

project(test_ADSR)

set(sdk_folder
../../../se_sdk3
)
set(sdk_srcs
${sdk_folder}/mp_sdk_common.cpp
${sdk_folder}/mp_sdk_audio.cpp
)
set(srcs
../Adsr.rc
../Adsr.cpp
test_ADSR.cpp
)
include_directories(
${sdk_folder}
)
include (GenerateExportHeader)

add_executable(${PROJECT_NAME} ${sdk_srcs} ${srcs})

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} "")

