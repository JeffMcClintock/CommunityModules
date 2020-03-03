cmake_minimum_required(VERSION 3.15)
project(CommunityModules)

set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DNDEBUG)
endif()

add_subdirectory(Bruno_Bordi)
add_subdirectory(JeffMcClintock)
