cmake_minimum_required(VERSION 3.15)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
set(SE_BUILDING_ON_CI FALSE CACHE BOOL "Environment is a build machine'")

project(CommunityModules)

enable_testing()

set(sdk_folder
    ../../se_sdk3
    )

include_directories(
    se_sdk3
    shared
    )

set(CMAKE_CXX_STANDARD 17)

add_definitions(-D_UNICODE)
add_definitions(-DSE_TARGET_SEM)
if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DNDEBUG)
endif()

if (MSVC)
    # Floating Point Model: Fast (/fp:fast)
    # Buffer Security Check: No (/GS-)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /GS-")
endif()

# include the standard source code files in the plugin, includes the sdk and plugin itself
# DSP and Gui and XML file names are generated from the project name
macro(
    set_plugin_standard_srcs
    PROJECT_NAME
    HAS_DSP
    HAS_GUI
)

set(sdk_srcs
${sdk_folder}/mp_sdk_common.h
${sdk_folder}/mp_sdk_common.cpp
)

if(${HAS_DSP})
    set(sdk_srcs ${sdk_srcs}
    ${sdk_folder}/mp_sdk_audio.h
    ${sdk_folder}/mp_sdk_audio.cpp
    )
    set(srcs ${srcs}
    ${PROJECT_NAME}.cpp
    )
endif()

if(${HAS_GUI})
    set(sdk_srcs ${sdk_srcs}
    ${sdk_folder}/mp_sdk_gui.h
    ${sdk_folder}/mp_sdk_gui.cpp
    )
    set(srcs ${srcs}
    ${PROJECT_NAME}Gui.cpp
    )
endif()

set(resource_srcs
module.rc
${PROJECT_NAME}.xml
)

# organise SDK file into folders/groups in IDE
source_group(sdk FILES ${sdk_srcs})
source_group(resources FILES ${resource_srcs})

endmacro()

# include the standard source code files in the plugin, includes the sdk and plugin itself
# DSP and Gui and XML file names are generated from the project name
macro(
    build_sem_plugin
    PROJECT_NAME
    HAS_DSP
    HAS_GUI
)

project(${PROJECT_NAME})

set_plugin_standard_srcs(
    ${PROJECT_NAME}
    ${HAS_DSP}
    ${HAS_GUI}
)

include (GenerateExportHeader)
add_library(${PROJECT_NAME} MODULE  ${srcs} ${sdk_srcs} ${resource_srcs})

if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE_EXTENSION "sem")

  # Place xml file in bundle 'Resources' folder.
  set(xml_path "${PROJECT_NAME}.xml")
  target_sources(${PROJECT_NAME} PUBLIC ${xml_path})
  set_source_files_properties(${xml_path} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".sem")

if(CMAKE_HOST_WIN32)
# maybe better: if(SE_COPY_TO_SEM_FOLDER)
if (NOT SE_BUILDING_ON_CI)
    add_custom_command(TARGET ${PROJECT_NAME}
    # Run after all other rules within the target have been executed
    POST_BUILD
    COMMAND xcopy /c /y "$(OutDir)$(TargetName)$(TargetExt)" "C:\\Program Files\\Common Files\\SynthEdit\\modules"
    COMMENT "Copy to system plugin folder"
    VERBATIM
)
endif()
endif()


endmacro()


add_subdirectory(Bruno_Bordi)
add_subdirectory(JeffMcClintock)
add_subdirectory(RJ)
