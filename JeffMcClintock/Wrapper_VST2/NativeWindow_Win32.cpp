#include <Windows.h>
#include "NativeWindow_Win32.h"
#include "VST3 SDK\aeffeditor.h"
#include "../shared/unicode_conversion.h"
#include "AEffectWrapper.h"

using namespace std;
using namespace JmUnicodeConversions;


vector< pair<HWND, NativeWindow_Win32*> > NativeWindow_Win32::hwndLookup;

NativeWindow_Win32::NativeWindow_Win32(class AEffectWrapper* pvstEffect) :
vstEffect(pvstEffect)
, hwnd_(0)
{
}


NativeWindow_Win32::~NativeWindow_Win32()
{
	if( hwnd_ )
	{
		SendMessage(hwnd_, WM_CLOSE, 0, 0);
	}
}

const wchar_t g_szClassName[] = L"SE_VST_Rapper_WindowClass";

// Step 4: the Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch( msg )
	{
	case WM_CLOSE:
		for (auto it = NativeWindow_Win32::hwndLookup.begin(); it != NativeWindow_Win32::hwndLookup.end(); ++it)
		{
			if ((*it).first == hwnd)
			{
				(*it).second->OnClose();
				break;
			}
		}
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		for( auto it = NativeWindow_Win32::hwndLookup.begin(); it != NativeWindow_Win32::hwndLookup.end(); ++it )
		{
			if( ( *it ).first == hwnd )
			{
				( *it ).second->OnDestroy();
				NativeWindow_Win32::hwndLookup.erase(it);
				break;
			}
		}
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// copied from MP_GetDllHandle
HMODULE local_GetDllHandle_randomshit2()
{
	HMODULE hmodule = 0;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&local_GetDllHandle_randomshit2, &hmodule);
	return (HMODULE)hmodule;
}

int NativeWindow_Win32::Open()
{
	// Get plugin window size.
	int width = 100;
	int height = 100;

	ERect* erp = NULL;
	auto res = vstEffect->dispatcher(effEditGetRect, 0, 0, &erp);

	if( erp != NULL ) // bombthebeat returns res = 0, although rect is valid.
	{
		width = erp->right - erp->left;
		height = erp->bottom - erp->top;
	}

	// Get plugin name.
	auto name = Utf8ToWstring( vstEffect->getName() );

	HINSTANCE hInstance = local_GetDllHandle_randomshit2();

	// Register the Window Class.
	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)( COLOR_WINDOW + 1 );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if( !RegisterClassEx(&wc) )
	{
		/* will fail 2nd time. TODO static init.
		MessageBox(NULL, L"Window Registration Failed!", L"Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
		*/
	}

	// Create Window.
	auto style = WS_OVERLAPPEDWINDOW&~( WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
	auto stylex = WS_EX_CLIENTEDGE;

	RECT r;
	r.left = 50;
	r.right = r.left + width;
	r.top = 50;
	r.bottom = r.top + height;

	::AdjustWindowRectEx(
		&r,
		style,
		FALSE,
		stylex
		);

	hwnd_ = CreateWindowEx(
		stylex,
		g_szClassName,
		name.c_str(),
		style,
		r.left, r.top, r.right - r.left, r.bottom - r.top,
		NULL, NULL, hInstance, NULL);

	if( hwnd_ == NULL )
	{
		MessageBox(NULL, L"Window Creation Failed!", L"Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	hwndLookup.push_back(pair<HWND, NativeWindow_Win32*>(hwnd_, this));

	ShowWindow(hwnd_, SW_SHOW);
	UpdateWindow(hwnd_);

	return 0;
}

void NativeWindow_Win32::Close()
{
	::SendMessage(hwnd_, WM_CLOSE, 0, 0);
}

void NativeWindow_Win32::OnDestroy()
{
	// Windows ideally destroyed on close, but just in case do it again.
	vstEffect->OnCloseEditor();
}

void NativeWindow_Win32::OnClose()
{
	vstEffect->OnCloseEditor();
}

void NativeWindow_Win32::ToFront()
{
	SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}