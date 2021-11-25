#pragma once
#include <functional>
#include <thread>
#include "../shared/xplatform.h"

#if defined(_WIN32)
#include <Windows.h>
typedef HANDLE nativeHandle;
#else
typedef int nativeHandle;
#endif

/* 
#include "../shared/FileWatcher.h"
*/

namespace file_watcher
{
	class FileWatcher
	{
		std::thread backgroundThread;
		nativeHandle stopEvent = {};

	public:
		void Start(const platform_string & fullPath, std::function<void(void)> callback);
		~FileWatcher();
	};
}

