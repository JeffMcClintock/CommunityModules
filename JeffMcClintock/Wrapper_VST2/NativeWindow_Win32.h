#pragma once
#include <vector>

class NativeWindow_Win32
{
public:
	NativeWindow_Win32(class AEffectWrapper* pvstEffect);
	~NativeWindow_Win32();
	int Open();
	void Close();

	void OnClose();
	void OnDestroy();
	void ToFront();

	class AEffectWrapper* vstEffect;
	HWND hwnd_;
	static std::vector< std::pair<HWND, NativeWindow_Win32*> > hwndLookup;
};

