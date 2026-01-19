# Wrapper_vst3 CMake Build Instructions

This project has been converted to use CMake as the build system.

## Prerequisites

- CMake 3.15 or higher
- Visual Studio 2019 or later (for Windows builds)
- VST3 SDK (default path: `../../../SE15/SDKs/VST3_SDK`)

## Building the Project

### Using CMake GUI

1. Open CMake GUI
2. Set source directory to: `JeffMcClintock/Wrapper_VST3`
3. Set build directory to: `JeffMcClintock/Wrapper_VST3/build`
4. Click "Configure" and select your Visual Studio version
5. Adjust `VST3_SDK_PATH` if your VST3 SDK is in a different location
6. Click "Generate"
7. Click "Open Project" or open the generated solution in Visual Studio

### Using Command Line

```powershell
# From the Wrapper_VST3 directory
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

For 32-bit builds:
```powershell
cmake -B build32 -G "Visual Studio 17 2022" -A Win32
cmake --build build32 --config Release
```

## Configuration Options

- `VST3_SDK_PATH`: Path to VST3 SDK (default: `${CMAKE_CURRENT_SOURCE_DIR}/../../../SE15/SDKs/VST3_SDK`)

You can set this when configuring:
```powershell
cmake -B build -DVST3_SDK_PATH="C:/path/to/VST3_SDK"
```

## Output

The built library will have a `.sem` extension and will be placed in:
- `build/bin/Debug/Wrapper_vst3.sem` (Debug builds)
- `build/bin/Release/Wrapper_vst3.sem` (Release builds)

## Post-Build Copy (Optional)

To automatically copy the built module to your SynthEdit modules folder, uncomment and adjust the post-build section in `CMakeLists.txt`:

```cmake
if(ARCH_64)
    set(SEM_MODULES_PATH "C:/Program Files/Common Files/SynthEdit/modules")
else()
    set(SEM_MODULES_PATH "C:/Program Files (x86)/Common Files/SynthEdit/modules")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${SEM_MODULES_PATH}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMENT "Copying ${PROJECT_NAME} to SEM modules folder"
)
```

## Notes

- The original Visual Studio project files (`.vcxproj`) are still available if needed
- CMake will automatically handle Debug/Release configurations
- The project uses C++17 standard
- All compiler and linker settings from the original project have been preserved
