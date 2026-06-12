cmake_minimum_required(VERSION 3.15)
project(SamplePlayer3Multi)

# -----------------------------------------------------------------------------
# Builds either standalone OR as part of the CommunityModules tree (via
# add_subdirectory). The heavy sfizz dependency is always fetched here; the GMPI
# SDK is only set up when building standalone -- in-tree the parent has already
# fetched GMPI, set GMPI_SDK and defined gmpi_plugin().
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

if(NOT DEFINED GMPI_SDK)
    # ---- standalone build: set up the GMPI SDK ourselves ----
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
    set(SE_LOCAL_BUILD FALSE CACHE BOOL "Execute extra build steps for developers machine")

    # Ensure NDEBUG is set for single-config (macOS command-line) builds.
    if(NOT GENERATOR_IS_MULTI_CONFIG)
        if(NOT CMAKE_BUILD_TYPE)
            set(CMAKE_BUILD_TYPE Release)
        endif()
    endif()

    # note: SOURCE_SUBDIR is a subfolder with NO cmake file (so we don't needlessly
    # include the GMPI examples)
    FetchContent_Declare(
        gmpi
        GIT_REPOSITORY https://github.com/JeffMcClintock/GMPI
        GIT_TAG origin/main
        SOURCE_SUBDIR Core
    )
    FetchContent_MakeAvailable(gmpi)

    set(GMPI_SDK ${gmpi_SOURCE_DIR})
    include(${gmpi_SOURCE_DIR}/gmpi_plugin.cmake)

    add_definitions(-D_UNICODE)
    add_definitions(-DSE_TARGET_SEM)

    if(MSVC)
        # Floating Point Model: Fast (/fp:fast); Buffer Security Check: No (/GS-).
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /GS-")
    endif()
endif()

# Static MSVC runtime, matched across the plugin and the sfizz sub-build.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# -----------------------------------------------------------------------------
# sfizz SFZ engine
# -----------------------------------------------------------------------------
FetchContent_Declare(
    sfizz
    GIT_REPOSITORY https://github.com/sfztools/sfizz.git
)

# disable building unwanted 'sfizz_render'
set(SFIZZ_RENDER OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(sfizz)

# -----------------------------------------------------------------------------
# Patch sfizz's vendored atomic_queue for newer (Xcode 16+) Apple Clang, which
# rejects the redundant 'template' disambiguator on these non-template member
# calls. Removing it is standard-conforming. Applied to the fetched source before
# it is compiled; idempotent (only rewrites when something actually changed).
# -----------------------------------------------------------------------------
if(APPLE)
    set(_atomic_queue_h "${sfizz_SOURCE_DIR}/external/atomic_queue/include/atomic_queue/atomic_queue.h")
    if(EXISTS "${_atomic_queue_h}")
        file(READ "${_atomic_queue_h}" _aq_orig)
        set(_aq_text "${_aq_orig}")
        string(REPLACE "Base::template do_pop_any"  "Base::do_pop_any"  _aq_text "${_aq_text}")
        string(REPLACE "Base::template do_push_any" "Base::do_push_any" _aq_text "${_aq_text}")
        if(NOT _aq_text STREQUAL _aq_orig)
            file(WRITE "${_atomic_queue_h}" "${_aq_text}")
            message(STATUS "Patched sfizz atomic_queue.h for strict Apple Clang")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Tuck sfizz's ~80 sub-targets (abseil, wavpack, aiff, kissfft, ...) into IDE
# solution folders so they don't clutter the Solution Explorer. Walk every
# target defined under the sfizz source tree and force its FOLDER (this overrides
# any folder the sub-projects set themselves).
# -----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

function(group_sfizz_targets dir)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _targets)
        if(_t MATCHES "^absl")
            set_target_properties(${_t} PROPERTIES FOLDER "Dependencies/abseil")
        elseif(_t MATCHES "^sfizz")
            set_target_properties(${_t} PROPERTIES FOLDER "Dependencies/sfizz")
        else()
            set_target_properties(${_t} PROPERTIES FOLDER "Dependencies/sfizz/3rdparty")
        endif()
    endforeach()

    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_sub IN LISTS _subdirs)
        group_sfizz_targets("${_sub}")
    endforeach()
endfunction()

group_sfizz_targets("${sfizz_SOURCE_DIR}")

# -----------------------------------------------------------------------------
# the plugin
# -----------------------------------------------------------------------------
gmpi_plugin(
    PROJECT_NAME ${PROJECT_NAME}
    HAS_DSP
    SOURCE_FILES
        ${PROJECT_NAME}.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE sfizz::sfizz)
