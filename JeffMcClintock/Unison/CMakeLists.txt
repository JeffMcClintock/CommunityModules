cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)

project(Unison)

gmpi_plugin(
	PROJECT_NAME ${PROJECT_NAME}
	HAS_DSP
	FORMATS_LIST
	 GMPI
	SOURCE_FILES
	  Unison.cpp
)
