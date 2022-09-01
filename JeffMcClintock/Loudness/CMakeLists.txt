cmake_minimum_required(VERSION 3.15)

project(Loudness)

build_gmpi_plugin(
    PROJECT_NAME ${PROJECT_NAME}
    HAS_DSP
    SOURCE_FILES 
        Ebu128LoudnessMeter.h
        Ebu128LoudnessMeter.cpp
        filters/SecondOrderIIRFilter.h
        filters/SecondOrderIIRFilter.cpp
)
