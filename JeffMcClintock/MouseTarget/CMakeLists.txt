cmake_minimum_required(VERSION 3.15)

project(MouseTarget)

build_gmpi_plugin(
    PROJECT_NAME ${PROJECT_NAME}
    HAS_GUI
    HAS_DSP
    SOURCE_FILES 
        ${PROJECT_NAME}Gui.cpp
        MathGui.cpp
        Image4Gui.h
        Image4Gui.cpp
        LabelGui.cpp
        ColorSwitchGui.cpp
        HexToColorGui.cpp
        ColorLagGui.cpp
        SvgImage.cpp
        tinyxml2/tinyxml2.h
        tinyxml2/tinyxml2.cpp
        ${se_shared_folder}/skinBitmap.h
        ${se_shared_folder}/ImageMetadata.h
        ${se_shared_folder}/ImageMetadata.cpp
        ${se_shared_folder}/ImageCache.h
        ${se_shared_folder}/ImageCache.cpp
        ${se_sdk_folder}/TimerManager.h
        ${se_sdk_folder}/TimerManager.cpp
)

source_group(sdk FILES
    ${se_shared_folder}/skinBitmap.h
    ${se_shared_folder}/ImageMetadata.h
    ${se_shared_folder}/ImageMetadata.cpp
    ${se_shared_folder}/ImageCache.h
    ${se_shared_folder}/ImageCache.cpp
    ${se_sdk_folder}/TimerManager.h
    ${se_sdk_folder}/TimerManager.cpp
)

# Support for timers on macOS
if(APPLE)
FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation )
MARK_AS_ADVANCED (COREFOUNDATION_LIBRARY)
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${COREFOUNDATION_LIBRARY} )
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
