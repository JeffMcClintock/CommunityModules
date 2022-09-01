cmake_minimum_required(VERSION 3.15)
project(db_to_voltage)

set(sdk_srcs
     ${sdk_folder}/mp_sdk_common.cpp
     ${sdk_folder}/mp_sdk_audio.cpp
    )

set(srcs
    db_to_voltage.cpp
    module.rc
    )

include (GenerateExportHeader)

add_library(${PROJECT_NAME} SHARED ${sdk_srcs} ${srcs})

target_compile_definitions(${PROJECT_NAME} PRIVATE 
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Release>:NDEBUG>
)

if(MAC)
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
elseif(WIN)
  set_property(TARGET ${PROJECT_NAME} PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()