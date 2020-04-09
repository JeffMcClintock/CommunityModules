cmake_minimum_required(VERSION 3.15)

#add_subdirectory(tests)

project(DrawingTest)
set(sdk_folder
../../se_sdk3
)
set(sdk_srcs
${sdk_folder}/mp_sdk_common.cpp
${sdk_folder}/mp_sdk_gui.cpp
${sdk_folder}/TimerManager.cpp
)
set(srcs
${PROJECT_NAME}.rc
DrawingTestGui.cpp
DrawingTestLinesGui.cpp
)
include_directories(
${sdk_folder}
)
include (GenerateExportHeader)
add_library(${PROJECT_NAME} MODULE ${sdk_srcs} ${srcs})

if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
  set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE_EXTENSION "sem")

  # Place module xml in bundle 'Resources' folder.
  set(xml_path "${PROJECT_NAME}.xml")
  target_sources(${PROJECT_NAME} PUBLIC ${xml_path})
  set_source_files_properties(${xml_path} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # Attempt: Place my Info.plist in bundle.
  # Might not be needed because CMake places a default Info.plist file in bundle anyhow.
  # XCODE_ATTRIBUTE_INFOPLIST_FILE ${CMAKE_CURRENT_BINARY_DIR}/mac/Info.plist
  # MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/mac/Info.plist

endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# TODO need this set on mac dll, else SE can't scan it.
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".sem")
