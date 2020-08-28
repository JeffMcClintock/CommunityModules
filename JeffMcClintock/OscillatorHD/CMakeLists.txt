cmake_minimum_required(VERSION 3.15)

#add_subdirectory(tests)

#Steinberg only: include(Bundle)

project(Oscillator)
set(sdk_folder
../../se_sdk3
)
set(sdk_srcs
${sdk_folder}/mp_sdk_common.cpp
${sdk_folder}/mp_sdk_audio.cpp
)
set(srcs
${PROJECT_NAME}.rc
${PROJECT_NAME}.cpp
real_fft.cpp
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

  # Place *.xml in bundle 'Resources' folder.
  target_sources(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}.xml)
  set_source_files_properties(${PROJECT_NAME}.xml PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # Attempt: Place my Info.plist in bundle.
  # Might not be needed because CMake places a default Info.plist file in bundle anyhow.
  # XCODE_ATTRIBUTE_INFOPLIST_FILE ${CMAKE_CURRENT_BINARY_DIR}/mac/Info.plist
  # MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/mac/Info.plist

# hOW TO PLACE English.lproj (or any file) in Resources
#set_source_files_properties(
#    ${CMAKE_CURRENT_BINARY_DIR}/InfoPlist.strings
#    ${CMAKE_CURRENT_BINARY_DIR}/Localized.rsrc
#    PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/English.lproj")

# things to try from VST3
#  smtg_set_bundle(${PROJECT_NAME} INFOPLIST "mac/Info.plist" PREPROCESS)
#        if(XCODE)
#            set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_WRAPPER_EXTENSION ${ARG_EXTENSION})
#        else()
#            set_target_properties(${target} PROPERTIES BUNDLE_EXTENSION ${ARG_EXTENSION})
#        endif()
# set_source_files_properties(${ARG_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
#        if(XCODE)
#            set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_INFOPLIST_FILE "${ARG_INFOPLIST}")
#            if(ARG_PREPROCESS)
#                set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_INFOPLIST_PREPROCESS "YES")
#            endif()
#            if(ARG_PREPROCESSOR_FLAGS)
#                set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_INFOPLIST_OTHER_PREPROCESSOR_FLAGS "${ARG_PREPROCESSOR_FLAGS}")
#            endif()
#        endif()

endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# TODO need this set on mac dll, else SE can't scan it.
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".sem")
