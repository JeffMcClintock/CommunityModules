#pragma once

/*
#include "../shared/xplatform.h"
*/

#include "./platform_string.h"

/*
  Platform				Defined
  Waves Plugin			SE_TARGET_WAVES
  SEM Plugin			SE_TARGET_SEM (selected SEMs)
  AU Plugin             SE_TARGET_AU					SE_TARGET_PLUGIN
  VST2/3 Plugin(64-bit) SE_TARGET_VST3					SE_TARGET_PLUGIN					SE_PLUGIN_SUPPORT
  JUCE Plugin			GMPI_IS_PLATFORM_JUCE==1		SE_TARGET_PLUGIN					SE_PLUGIN_SUPPORT
  VST2 Plugin (32-bit)	SE_TARGET_VST2					SE_TARGET_PLUGIN	SE_SUPPORT_MFC	SE_PLUGIN_SUPPORT                  // V1.4 and earlier only
  SynthEdit.exe			SE_EDIT_SUPPORT					SE_SUPPORT_MFC	SE_PLUGIN_SUPPORT
  SynthEdit.exe (store)	SE_EDIT_SUPPORT					SE_SUPPORT_MFC	SE_PLUGIN_SUPPORT  SE_TARGET_UWP
  
  SynthEdit Universal	SE_TARGET_WINDOWS_STORE_APP
  Win Store App			SE_TARGET_WINDOWS_STORE_APP

  SE_PLUGIN_SUPPORT - SEM Version2 dll support.
  SE_TARGET_PLUGIN  - Code is running in a plugin (VST, AU JUCE etc)
  SE_SUPPORT_MFC    - MFC-based GUI support (SynthEdit 1, VST2).
*/

#if (defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP )
#define SE_TARGET_WINDOWS_STORE_APP
#endif

#if defined(JUCE_APP_VERSION)
#define GMPI_IS_PLATFORM_JUCE 1
#else
#define GMPI_IS_PLATFORM_JUCE 0
#endif

// SE_SUPPORT_MFC = VST 2.4
#if (GMPI_IS_PLATFORM_JUCE == 0) && !defined(SE_SUPPORT_MFC) && !defined(SE_TARGET_VST3) && !defined(SE_TARGET_AU) && !defined(SE_TARGET_SEM) && !defined(SE_EDIT_SUPPORT) && !defined(SE_TARGET_WINDOWS_STORE_APP)
    #define SE_TARGET_WAVES
	#define GMPI_IS_PLATFORM_WV 1
#else
	#define GMPI_IS_PLATFORM_WV 0
#endif

// External plugins not supported on Waves, and Not supported on Windows store Apps
#if !defined( SE_TARGET_WAVES ) && !defined(SE_TARGET_WINDOWS_STORE_APP) && !GMPI_IS_PLATFORM_JUCE
	#define SE_EXTERNAL_SEM_SUPPORT
#endif

#if defined( SE_TARGET_VST3 ) || defined(SE_TARGET_AU) || GMPI_IS_PLATFORM_JUCE
	#define SE_TARGET_PLUGIN 1
#endif

// MFC Support
#if defined( SE_TARGET_VST2 ) || (defined( SE_EDIT_SUPPORT ) && !defined(SE_TARGET_WINDOWS_STORE_APP))
#if !defined(SE_SUPPORT_MFC)
#error please define SE_SUPPORT_MFC
#endif
#endif

#ifdef _WIN32
	#define PLATFORM_PATH_SLASH '\\'
	#define PLATFORM_PATH_SLASH_L L'\\'
#else
	#define PLATFORM_PATH_SLASH '/'
	#define PLATFORM_PATH_SLASH_L L'/'
    #define MAX_PATH 500
#endif
