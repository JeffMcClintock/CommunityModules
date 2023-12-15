cmake_minimum_required(VERSION 3.15)

project(MouseTarget)

build_gmpi_plugin(
    PROJECT_NAME ${PROJECT_NAME}
    HAS_GUI
    SOURCE_FILES 
        ${PROJECT_NAME}Gui.cpp
        MathGui.cpp
        Image4Gui.h
        Image4Gui.cpp
        ${se_shared_folder}/skinBitmap.h
        ${se_shared_folder}/ImageMetadata.h
        ${se_shared_folder}/ImageMetadata.cpp
        ${se_shared_folder}/ImageCache.h
        ${se_shared_folder}/ImageCache.cpp
)
