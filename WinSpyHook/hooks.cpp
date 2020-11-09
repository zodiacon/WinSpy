#include "pch.h"

#pragma data_seg(".shared")
__declspec(dllexport) HHOOK g_hooks[WH_MAX + 1] = { 0 };
__declspec(dllexport) DWORD g_MainTid;
#pragma data_seg()
#pragma comment(linker, "/section:.shared,RWS")

extern "C" __declspec(dllexport)
LRESULT WINAPI CBTHook(int code, WPARAM wParam, LPARAM lParam) {
	return ::CallNextHookEx(g_hooks[WH_CBT], code, wParam, lParam);
}
