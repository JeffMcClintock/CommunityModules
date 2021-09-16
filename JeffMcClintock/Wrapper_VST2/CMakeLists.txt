cmake_minimum_required(VERSION 3.15)

#add_subdirectory(tests)

#Steinberg only: include(Bundle)

project(Wrapper_VST2)
set(sdk_folder
../../se_sdk3
)
set(sdk_shared_folder
../../shared
)
set(sdk_srcs
${sdk_folder}/mp_sdk_common.cpp
${sdk_folder}/mp_sdk_audio.cpp
${sdk_folder}/mp_sdk_gui.cpp
${sdk_folder}/mp_sdk_gui2.cpp
${sdk_folder}/TimerManager.cpp
${sdk_shared_folder}/FileFinder.cpp
${sdk_shared_folder}/xp_dynamic_linking
)

set(srcs
${PROJECT_NAME}.rc
${PROJECT_NAME}.cpp
AaVstWrapperDiagGui.h
VST2WrapperController.h
VstwrapperfailGui.h
NativeWindow_Win32.h
PluginLifetimeManager.h
Vst2WrapperGui.h
AEffectWrapper.h
VstFactory.h
Wrapper_VST2.h
VstwrapperfailGui.cpp
VstFactory.cpp
Vst2WrapperGui.cpp
VST2WrapperController.cpp
Wrapper_VST2.cpp
PluginLifetimeManager.cpp
NativeWindow_Win32.cpp
AEffectWrapper.cpp
AaVstWrapperDiagGui.cpp
)
include_directories(
${sdk_folder}
${sdk_shared_folder}
)

source_group(se_sdk FILES ${sdk_srcs})
source_group("Resource Files" FILES ${PROJECT_NAME}.rc)

include (GenerateExportHeader)
add_library(${PROJECT_NAME} MODULE ${sdk_srcs} ${srcs})
add_definitions(-DCOMPILE_HOST_SUPPORT)

if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE_EXTENSION "sem")

  # Place *.xml in bundle 'Resources' folder.
  target_sources(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}.xml)
  set_source_files_properties(${PROJECT_NAME}.xml PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# TODO need this set on mac dll, else SE can't scan it.
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".sem")
