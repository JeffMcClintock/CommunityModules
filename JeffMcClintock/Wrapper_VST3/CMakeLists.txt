cmake_minimum_required(VERSION 3.30)
project(Wrapper_vst3 VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# CPM bootstrap (shared cache across builds)
set(CPM_DOWNLOAD_VERSION 0.38.6)
set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)
set(CPM_GIT_SHALLOW ON) # shallow clone for speed

if(NOT DEFINED CPM_SOURCE_CACHE)
  if(WIN32)
    set(CPM_SOURCE_CACHE "$ENV{USERPROFILE}/.cpm" CACHE STRING "CPM cache dir")
  else()
    set(CPM_SOURCE_CACHE "$ENV{HOME}/.cache/CPM" CACHE STRING "CPM cache dir")
  endif()
endif()

set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
  file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
    "${CPM_DOWNLOAD_LOCATION}"
    TLS_VERIFY ON
  )
endif()
include("${CPM_DOWNLOAD_LOCATION}")

# Fetch VST3 SDK via CPM
CPMAddPackage(
NAME vst3_sdk
GITHUB_REPOSITORY steinbergmedia/vst3sdk
GIT_TAG v3.7.14_build_55       # or a specific commit SHA
DOWNLOAD_ONLY YES              # we only need sources; actual add_subdirectory happens later
)
set(VST3_SDK_PATH ${vst3_sdk_SOURCE_DIR})

# SYNTHEDIT SDK
message(STATUS "Fetching GMPI from github")
#note: SOURCE_SUBDIR is a subfolder with NO cmake file
FetchContent_Declare(
  se_sdk3
  GIT_REPOSITORY https://github.com/JeffMcClintock/SynthEdit_SDK
  GIT_TAG origin/master
  SOURCE_SUBDIR help
)
FetchContent_MakeAvailable(se_sdk3)
set(SE_SDK_PATH ${se_sdk3_SOURCE_DIR}/se_sdk3)
set(SE_SHARED_PATH ${se_sdk3_SOURCE_DIR}/shared)

# Main project files
set(MAIN_SOURCE_FILES
    VstFactory.cpp
    AaVstWrapperDiagGui.cpp
    VstwrapperfailGui.cpp
    ControllerWrapper.cpp
    EditButtonGui.cpp
    ProcessorWrapper.cpp
    WindowManager.cpp
    my_input_stream.cpp
)

set(MAIN_HEADER_FILES
    resource.h
    VstFactory.h
    AaVstWrapperDiagGui.h
    VstwrapperfailGui.h
    ControllerWrapper.h
    EditButtonGui.h
    ProcessorWrapper.h
    WindowManager.h
    myPluginProvider.h
    lock_free_fifo.h
    my_input_stream.h
    my_msg_que_input_stream.h
    my_msg_que_output_stream.h
)

# SE SDK3 files
set(SE_SDK_SOURCE_FILES
    ${SE_SDK_PATH}/mp_sdk_audio.cpp
    ${SE_SDK_PATH}/mp_sdk_common.cpp
    ${SE_SDK_PATH}/mp_sdk_gui2.cpp
    ${SE_SDK_PATH}/mp_sdk_gui.cpp
    ${SE_SDK_PATH}/TimerManager.cpp
)

set(SE_SDK_HEADER_FILES
    ${SE_SDK_PATH}/mp_sdk_audio.h
    ${SE_SDK_PATH}/mp_sdk_common.h
    ${SE_SDK_PATH}/mp_sdk_controller.h
    ${SE_SDK_PATH}/mp_sdk_gui2.h
    ${SE_SDK_PATH}/mp_sdk_gui.h
    ${SE_SDK_PATH}/TimerManager.h
)

# SE Shared files
set(SE_SHARED_SOURCE_FILES
    ${SE_SHARED_PATH}/xp_dynamic_linking.cpp
    ${SE_SHARED_PATH}/FileFinder.cpp
)

set(SE_SHARED_HEADER_FILES
    ${SE_SHARED_PATH}/xp_dynamic_linking.h
    ${SE_SHARED_PATH}/FileFinder.h
    ${SE_SHARED_PATH}/string_utilities.h
    ${SE_SHARED_PATH}/unicode_conversion.h
)

# VST3 SDK files
set(VST3_SOURCE_FILES
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/module.cpp
    ${VST3_SDK_PATH}/public.sdk/source/vst/utility/stringconvert.cpp
    ${VST3_SDK_PATH}/public.sdk/source/common/commonstringconvert.h
    ${VST3_SDK_PATH}/public.sdk/source/common/commonstringconvert.cpp
    ${VST3_SDK_PATH}/pluginterfaces/base/coreiids.cpp
    ${VST3_SDK_PATH}/base/source/fobject.cpp
    ${VST3_SDK_PATH}/public.sdk/source/vst/vstinitiids.cpp
    ${VST3_SDK_PATH}/pluginterfaces/base/funknown.cpp
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/connectionproxy.cpp
    ${VST3_SDK_PATH}/base/source/fdebug.cpp
    ${VST3_SDK_PATH}/base/thread/source/flock.cpp
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/hostclasses.cpp
    ${VST3_SDK_PATH}/base/source/fstring.cpp
    ${VST3_SDK_PATH}/public.sdk/source/common/commoniids.cpp
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
    ${VST3_SDK_PATH}/base/source/baseiids.cpp
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/processdata.cpp
    ${VST3_SDK_PATH}/base/source/updatehandler.cpp
)

set(VST3_HEADER_FILES
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/module.h
    ${VST3_SDK_PATH}/public.sdk/source/vst/utility/stringconvert.h
    ${VST3_SDK_PATH}/base/source/fobject.h
    ${VST3_SDK_PATH}/pluginterfaces/base/funknown.h
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/connectionproxy.h
    ${VST3_SDK_PATH}/base/source/fdebug.h
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/hostclasses.h
    ${VST3_SDK_PATH}/base/source/fstring.h
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/pluginterfacesupport.h
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/processdata.h
    ${VST3_SDK_PATH}/base/source/updatehandler.h
)

if(WIN32)
set(VST3_SOURCE_FILES ${VST3_SOURCE_FILES}
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/module_win32.cpp
    ${VST3_SDK_PATH}/public.sdk/source/common/threadchecker_win32.cpp
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/win32/platform.cpp
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/win32/window.cpp
)

set(VST3_HEADER_FILES ${VST3_HEADER_FILES}
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/win32/window.h
)
endif()

if(APPLE)
set(VST3_SOURCE_FILES ${VST3_SOURCE_FILES}
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/module_mac.mm
    ${VST3_SDK_PATH}/public.sdk/source/common/threadchecker_mac.mm
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/mac/platform.mm
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/mac/window.mm
)
set(VST3_HEADER_FILES ${VST3_HEADER_FILES}
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/mac/window.h
)

# Ensure ARC for Objective-C++ sources
set(OBJC_ARC_SOURCES
    ${VST3_SDK_PATH}/public.sdk/source/vst/hosting/module_mac.mm
    ${VST3_SDK_PATH}/public.sdk/source/common/threadchecker_mac.mm
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/mac/platform.mm
    ${VST3_SDK_PATH}/public.sdk/samples/vst-hosting/editorhost/source/platform/mac/window.mm
)
set_source_files_properties(${OBJC_ARC_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()


# Combine all files for library target
set(SOURCE_FILES
    ${MAIN_SOURCE_FILES}
    ${SE_SDK_SOURCE_FILES}
    ${SE_SHARED_SOURCE_FILES}
    ${VST3_SOURCE_FILES}
)

set(HEADER_FILES
    ${MAIN_HEADER_FILES}
    ${SE_SDK_HEADER_FILES}
    ${SE_SHARED_HEADER_FILES}
    ${VST3_HEADER_FILES}
)

# Resource files
set(RESOURCE_FILES
    Wrapper_vst3.rc
)

# Create the library
add_library(${PROJECT_NAME} SHARED
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${RESOURCE_FILES}
)

# ???
# Set output properties
#set_target_properties(${PROJECT_NAME} PROPERTIES
#    OUTPUT_NAME "Wrapper_vst3"
#    SUFFIX ".sem"
#    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
#)

if(APPLE)
set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE_EXTENSION vst3)
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../../se_sdk3
    ${VST3_SDK_PATH}
)

# Preprocessor definitions
# 'DEBUG' is a steinberg thing, ('_DEBUG' is standard C++)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    COMPILE_HOST_SUPPORT
    SE_TARGET_SEM
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:_DEBUG;_WINDOWS;_USRDLL;DEBUG=1>
    $<$<CONFIG:Release>:NDEBUG;_WINDOWS;_USRDLL;DEBUG=0;_SECURE_SCL=0>
    $<$<BOOL:${WIN32}>:WIN32>
    $<$<BOOL:${ARCH_64}>:_CRT_SECURE_NO_WARNINGS>
)

# Compiler options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3                          # Warning level 3
        /fp:fast                     # Fast floating point model
        /GS-                         # Disable buffer security checks
        $<$<CONFIG:Debug>:/Od /RTC1> # Debug: No optimization, runtime checks
        $<$<CONFIG:Release>:/O2 /Ob2> # Release: Full optimization, inline expansion
    )
else()
    # Clang/GCC options
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall                        # Enable most warnings
        -ffast-math                  # Fast floating point model
        $<$<CONFIG:Debug>:-O0 -g>    # Debug: No optimization, debug info
        $<$<CONFIG:Release>:-O2>     # Release: Full optimization
    )
endif()

# Enable ARC for Objective-C/Objective-C++ files on Apple platforms
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OBJC_ARC ON
    )
    find_library(APPKIT_FRAMEWORK AppKit)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${APPKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )
endif()

# Linker options
if(WIN32)
    target_link_options(${PROJECT_NAME} PRIVATE
        /SUBSYSTEM:WINDOWS
        $<$<NOT:$<BOOL:${ARCH_64}>>:/SAFESEH:NO>  # Disable safe exception handlers for 32-bit
    )
endif()

# Runtime library settings
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

# Post-build event - copy to SEM modules folder (optional)
# Uncomment and adjust path as needed
# if(ARCH_64)
#     set(SEM_MODULES_PATH "C:/Program Files/Common Files/SynthEdit/modules")
# else()
#     set(SEM_MODULES_PATH "C:/Program Files (x86)/Common Files/SynthEdit/modules")
# endif()
# 
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "$<TARGET_FILE:${PROJECT_NAME}>"
#         "${SEM_MODULES_PATH}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
#     COMMENT "Copying ${PROJECT_NAME} to SEM modules folder"
# )

# IDE folder organization
# Apply source groups
source_group("SE_SDK\\Source Files" FILES ${SE_SDK_SOURCE_FILES})
source_group("SE_SDK\\Header Files" FILES ${SE_SDK_HEADER_FILES})
source_group("SE_SDK\\Shared\\Source Files" FILES ${SE_SHARED_SOURCE_FILES})
source_group("SE_SDK\\Shared\\Header Files" FILES ${SE_SHARED_HEADER_FILES})
source_group("VST3_SDK\\Source Files" FILES ${VST3_SOURCE_FILES})
source_group("VST3_SDK\\Header Files" FILES ${VST3_HEADER_FILES})
source_group("Source Files" FILES ${MAIN_SOURCE_FILES})
source_group("Header Files" FILES ${MAIN_HEADER_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES})





