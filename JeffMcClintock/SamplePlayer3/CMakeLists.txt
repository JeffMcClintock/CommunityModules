cmake_minimum_required(VERSION 3.15)
project(SamplePlayerSfz)

############################ COPIED FROM CM ###########################

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE) 
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
set(SE_LOCAL_BUILD FALSE CACHE BOOL "Execute extra build steps for developers machine")
set(IS_OFFICIAL_MODULE TRUE)

# This is for macOS commandline only, because it uses a single-target generator. for other targets, ref: target_compile_definitions
# this point of this is to ensure NDEBUG macro is set
if(NOT GENERATOR_IS_MULTI_CONFIG)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
endif()

include(FetchContent)

#note: SOURCE_SUBDIR is a subfolder with NO cmake file (so we don't needlessly include the GMPI examples)

FetchContent_Declare(
  gmpi
  GIT_REPOSITORY https://github.com/JeffMcClintock/GMPI
  GIT_TAG origin/main
  SOURCE_SUBDIR  Core
)

FetchContent_MakeAvailable(gmpi)

#project(CommunityModules)

enable_testing()

set(se_sdk_folder
    ../../se_sdk3
    )

set(gmpi_sdk_folder
    ${CMAKE_BINARY_DIR}/_deps/gmpi-src/Core
    )
	
include_directories(
    se_sdk3
    shared
    )

set(CMAKE_CXX_STANDARD 17)

add_definitions(-D_UNICODE)
add_definitions(-DSE_TARGET_SEM)

if (MSVC)
    # Floating Point Model: Fast (/fp:fast)
    # Buffer Security Check: No (/GS-)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /GS-")
endif()

#include(plugin_helper.cmake)
include(${CMAKE_BINARY_DIR}/_deps/gmpi-src/gmpi_plugin.cmake)

#######################################################################
include(plugin_helper_local.cmake)

# fetch the SFIZZ library
FetchContent_Declare(
  sfizz
  GIT_REPOSITORY https://github.com/sfztools/sfizz.git
)

# disable building unwanted 'sfizz_render''
set(SFIZZ_RENDER OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(sfizz)

set(sdk_folder
../../se_sdk3
)
set(sdk_srcs
${sdk_folder}/mp_sdk_common.cpp
${sdk_folder}/mp_sdk_audio.cpp
)
set(srcs
module.rc
SamplePlayerSfz.cpp
)

include_directories(
${sdk_folder}
../../shared
)

include (GenerateExportHeader)

add_library(${PROJECT_NAME} MODULE ${sdk_srcs} ${srcs})

target_link_libraries(${PROJECT_NAME}
    PRIVATE sfizz::sfizz
)

# DIDN'T WORK
# source_group(TREE ${sfizz_SOURCE_DIR} PREFIX "SFIZLIB")
# message("XXXXXXX")
# message(${sfizz_SOURCE_DIR})

if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE_EXTENSION "sem")

  # Place xml in bundle 'Resources' folder.
  set(xml_path "SamplePlayerSfz.xml")
  target_sources(${PROJECT_NAME} PUBLIC ${xml_path})
  set_source_files_properties(${xml_path} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".sem")

if(CMAKE_HOST_WIN32)

if (SE_LOCAL_BUILD)
if(${IS_OFFICIAL_MODULE})
    add_custom_command(TARGET ${PROJECT_NAME}
    # Run after all other rules within the target have been executed
    POST_BUILD
    COMMAND xcopy /c /y "\"$(OutDir)$(TargetName)$(TargetExt)\"" "\"C:\\SE\\SE15\\SynthEdit\\modules64\\\""
    COMMENT "Copy to SynthEdit plugin folder"
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME}
    # Run after all other rules within the target have been executed
    POST_BUILD
    COMMAND xcopy /c /y "\"$(OutDir)$(TargetName)$(TargetExt)\"" "\"C:\\Program Files\\Common Files\\SynthEdit\\modules\\\""
    COMMENT "Copy to SynthEdit plugin folder"
    )
endif()
endif()
endif()
