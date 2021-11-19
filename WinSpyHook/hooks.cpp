#include "pch.h"
#include "hooks.h"

const unsigned MaxHooks = 8;

struct HookEntry {
	HHOOK hHook;
	HWND CallbackWnd;
	DWORD ThreadId;
	HWND TargetHwnd;
	HookOptions Options;
};

#pragma data_seg(".shared")
unsigned g_HookCount = 0;
HookEntry g_HookMap[3][MaxHooks] {};
#pragma data_seg()
#pragma comment(linker, "/section:.shared,RWS")

bool IsNotify(HWND hTarget, HookEntry const& entry) {
	if ((entry.Options & HookOptions::Thread) == HookOptions::Thread)
		return true;

	if ((entry.Options & HookOptions::Window) == HookOptions::Window && hTarget == entry.TargetHwnd)
		return true;

	//
	// check if child window
	//
	return entry.TargetHwnd == ::GetAncestor(hTarget, GA_PARENT);
}

LRESULT WINAPI HookFunc3(int code, WPARAM wParam, LPARAM lParam) {
	if (code == HC_ACTION) {
		auto msg = reinterpret_cast<MSG*>(lParam);
		auto hWnd =  msg->hwnd;
		auto tid = ::GetWindowThreadProcessId(hWnd, nullptr);
		ATLTRACE(L"HookFunc3: hWnd: 0x%p TID: %u\n", hWnd, tid);
		for (auto& hook : g_HookMap[0]) {
			if (hook.ThreadId == tid) {
				if (IsNotify(hWnd, hook)) {
					ATLTRACE(L"HookFunc3: Sending message to 0x%p (%u)\n", hook.CallbackWnd, WM_HOOKCALLBACK);
					COPYDATASTRUCT cds;
					GetMessageData data;
					data.Msg = *msg;
					data.HookType = WH_GETMESSAGE;
					data.wParam = wParam;
					cds.cbData = sizeof(data);
					cds.lpData = &data;
					::SendMessage(hook.CallbackWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
				}
			}
		}
	}
	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT WINAPI HookFunc4(int code, WPARAM wParam, LPARAM lParam) {

	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT WINAPI HookFunc12(int code, WPARAM wParam, LPARAM lParam) {

	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

bool WINAPI AddHook(DWORD hookType, HookConfig const& config) {
	if (g_HookCount == MaxHooks)
		return false;

	extern HINSTANCE g_hInstDll;
	CStringA name;
	name.Format("HookFunc%u", hookType);
	auto proc = (HOOKPROC)::GetProcAddress(g_hInstDll, name);
	ATLASSERT(proc);
	auto hHook = ::SetWindowsHookEx(hookType, proc, g_hInstDll, config.ThreadId);
	if (!hHook)
		return false;

	auto index = -1;
	switch (hookType) {
		case WH_GETMESSAGE: index = 0; break;
		case WH_CALLWNDPROC: index = 1; break;
		case WH_CALLWNDPROCRET: index = 2; break;
		default:
			ATLASSERT(false);
			break;
	}
	if (index < 0)
		return false;

	HookEntry entry{ hHook, config.CallbackWnd, config.ThreadId, config.TargetWnd, config.Options };
	//std::lock_guard locker(g_Lock);
	g_HookMap[index][g_HookCount++] = entry;
	return true;
}

bool WINAPI RemoveHook(DWORD tid) {
	//std::lock_guard locker(g_Lock);
	for (int i = 0; i < _countof(g_HookMap); i++) {
		for(auto& hook : g_HookMap[i]) {
			if (hook.ThreadId == tid) {
				::UnhookWindowsHookEx(hook.hHook);
				hook.ThreadId = 0;
				break;
			}
		}
	}

	return false;
}
