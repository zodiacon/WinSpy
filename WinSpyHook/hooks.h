#pragma once

const UINT WM_HOOKCALLBACK = WM_USER + 333;

enum class HookOptions {
	None = 0,
	Window = 1,
	ChildWindows = 2,
	Thread = 4,
};
DEFINE_ENUM_FLAG_OPERATORS(HookOptions);

struct HookConfig {
	HWND TargetWnd;
	DWORD ThreadId;
	HWND CallbackWnd;
	HookOptions Options;
};

bool WINAPI AddHook(DWORD hookType, HookConfig const& config);
bool WINAPI RemoveHook(HWND callbackWnd);

//
// a filter applied in the target, so messages that would not be shown don't cost it a cross-process call:
// a bit for each message below FilterLowMessages (set: send it), and one flag for all the others
//
const UINT FilterLowMessages = WM_USER;
bool WINAPI SetHookFilter(HWND callbackWnd, DWORD hookType, BYTE const* lowMessages, BOOL highMessages);

struct HookDataHeader {
	DWORD HookType;
};

struct GetMessageData : HookDataHeader {
	WPARAM wParam;
	MSG Msg;
};

//
// what a sent message's pointer parameter pointed to (copied in the target process, as the pointer means nothing outside it)
//
enum class ExtraDataType : DWORD {
	None,
	Text,			// WCHAR[], not terminated (ExtraSize says how long)
	WindowPos,		// WINDOWPOS
	Notify,			// NMHDR
	Style,			// STYLESTRUCT
	MinMaxInfo,		// MINMAXINFO
	Create,			// CreateData
	Rect,			// RECT
};

struct CreateData {
	int x, y, cx, cy;
	LONG Style;
	DWORD ExStyle;
	WCHAR Name[64];		// empty if there is no name (or it's a resource ID)
};

const DWORD MaxExtraData = 512;

// WH_CALLWNDPROC (sent) and WH_CALLWNDPROCRET (returned) messages
struct CallWndProcData : HookDataHeader {
	HWND hWnd;
	UINT Message;
	WPARAM wParam;
	LPARAM lParam;
	LRESULT Result;			// WH_CALLWNDPROCRET only
	DWORD Time;
	// the hook's wParam: nonzero when the message came from another thread (the opposite of what the
	// documentation says, but what Windows does)
	BOOL FromOtherThread;
	ExtraDataType ExtraType;
	DWORD ExtraSize;
	BYTE Extra[MaxExtraData];
};
