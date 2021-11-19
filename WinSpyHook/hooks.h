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
bool WINAPI RemoveHook(DWORD tid);

struct HookDataHeader {
	DWORD HookType;
};

struct GetMessageData : HookDataHeader {
	WPARAM wParam;
	MSG Msg;
};
