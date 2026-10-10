#pragma once

#include "hooks.h"

struct HookHelper final {
	static bool InitHookLib();
	static bool WINAPI AddHook(DWORD hookType, HookConfig const& config);
	static bool WINAPI RemoveHook(HWND callbackWnd);
	static bool WINAPI SetHookFilter(HWND callbackWnd, DWORD hookType, BYTE const* lowMessages, BOOL highMessages);

};

