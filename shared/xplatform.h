#pragma once

/*
#include "../shared/xplatform.h"
*/

#include "./platform_string.h"

/*
* todo: bETTER TO SUPPORT FEATURES, e.g. 'SEM support' NOT 'JUCE' or 'VST3'
* 
  Platform				Defined
  SEM Plugin			SE_TARGET_SEM (selected SEMs)
  AU Plugin             SE_TARGET_AU					SE_TARGET_PLUGIN
  VST2/3 Plugin(64-bit) SE_TARGET_VST3					SE_TARGET_PLUGIN
  JUCE Plugin			GMPI_IS_PLATFORM_JUCE==1		SE_TARGET_PLUGIN
  SynthEdit.exe			SE_EDIT_SUPPORT					SE_SUPPORT_MFC
  SynthEdit.exe (store)	SE_EDIT_SUPPORT					SE_SUPPORT_MFC	  SE_TARGET_STORE
  
  SynthEdit Universal (UWP App)	SE_TARGET_PURE_UWP

  SE_TARGET_PLUGIN  - Code is running in a plugin (VST, AU, JUCE etc)
  SE_SUPPORT_MFC    - MFC-based Serialization support.
*/

#if (defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP )
#define SE_TARGET_PURE_UWP
#endif

#if defined(JUCE_APP_VERSION)
#define GMPI_IS_PLATFORM_JUCE 1
#else
#define GMPI_IS_PLATFORM_JUCE 0
#endif

// External plugins not supported on Windows store Apps
#if !defined(SE_TARGET_PURE_UWP) && !GMPI_IS_PLATFORM_JUCE
	#define SE_EXTERNAL_SEM_SUPPORT
#endif

#if defined( SE_TARGET_VST3 ) || defined(SE_TARGET_AU) || GMPI_IS_PLATFORM_JUCE
	// The SDK references this macro (without including xplatform.h), so you need to explicity define it in the IDE/Build system.
	#if !defined(SE_TARGET_PLUGIN)
		#error please define SE_TARGET_PLUGIN
	#endif
#endif

// MFC Support
#if defined( SE_EDIT_SUPPORT ) && !defined(SE_TARGET_PURE_UWP)
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
