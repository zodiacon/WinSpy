#include "pch.h"
#include "hooks.h"

const unsigned MaxHooks = 8;

struct HookEntry {
	HHOOK hHook;
	HWND CallbackWnd;
	LONG volatile ThreadId;
	HWND TargetHwnd;
	HookOptions Options;
};

//
// a slot is free when its hHook is null; ThreadId is set last when adding and cleared first when removing,
// so the hook procedures (which match on it) never see a half-built entry
//
#pragma data_seg(".shared")
HookEntry g_HookMap[3][MaxHooks] {};
#pragma data_seg()
#pragma comment(linker, "/section:.shared,RWS")

bool IsNotify(HWND hTarget, HookEntry const& entry) {
	if ((entry.Options & HookOptions::Thread) == HookOptions::Thread)
		return true;

	if ((entry.Options & HookOptions::Window) == HookOptions::Window && hTarget == entry.TargetHwnd)
		return true;

	//
	// any descendant of the target, not just its direct children
	//
	return (entry.Options & HookOptions::ChildWindows) == HookOptions::ChildWindows && ::IsChild(entry.TargetHwnd, hTarget);
}

LRESULT WINAPI HookFunc3(int code, WPARAM wParam, LPARAM lParam) {
	//
	// only messages that are taken off the queue; peeks (PM_NOREMOVE) would show the same message again
	//
	if (code == HC_ACTION && (wParam & PM_REMOVE)) {
		auto msg = reinterpret_cast<MSG*>(lParam);
		auto hWnd =  msg->hwnd;
		auto tid = ::GetWindowThreadProcessId(hWnd, nullptr);
		ATLTRACE(L"HookFunc3: hWnd: 0x%p TID: %u\n", hWnd, tid);
		for (auto& hook : g_HookMap[0]) {
			if (tid && (DWORD)hook.ThreadId == tid) {
				if (IsNotify(hWnd, hook)) {
					ATLTRACE(L"HookFunc3: Sending message to 0x%p (%u)\n", hook.CallbackWnd, WM_HOOKCALLBACK);
					COPYDATASTRUCT cds;
					GetMessageData data;
					data.Msg = *msg;
					data.HookType = WH_GETMESSAGE;
					data.wParam = wParam;
					cds.cbData = sizeof(data);
					cds.lpData = &data;
					// this runs on the target's thread: don't let a stalled WinSpy hang it
					::SendMessageTimeout(hook.CallbackWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
						SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 200, nullptr);
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

	//
	// claim a free slot (other WinSpy instances share this map)
	//
	auto const claimed = reinterpret_cast<HHOOK>(1);
	HookEntry* slot = nullptr;
	for (auto& hook : g_HookMap[index]) {
		if (::InterlockedCompareExchangePointer((PVOID*)&hook.hHook, claimed, nullptr) == nullptr) {
			slot = &hook;
			break;
		}
	}
	if (!slot)
		return false;

	extern HINSTANCE g_hInstDll;
	CStringA name;
	name.Format("HookFunc%u", hookType);
	auto proc = (HOOKPROC)::GetProcAddress(g_hInstDll, name);
	ATLASSERT(proc);
	auto hHook = proc ? ::SetWindowsHookEx(hookType, proc, g_hInstDll, config.ThreadId) : nullptr;
	if (!hHook) {
		::InterlockedExchangePointer((PVOID*)&slot->hHook, nullptr);
		return false;
	}

	slot->CallbackWnd = config.CallbackWnd;
	slot->TargetHwnd = config.TargetWnd;
	slot->Options = config.Options;
	::InterlockedExchangePointer((PVOID*)&slot->hHook, hHook);
	::InterlockedExchange(&slot->ThreadId, (LONG)config.ThreadId);
	return true;
}

bool WINAPI RemoveHook(HWND callbackWnd) {
	bool removed = false;
	for (auto& row : g_HookMap) {
		for (auto& hook : row) {
			if (hook.ThreadId && hook.CallbackWnd == callbackWnd) {
				::InterlockedExchange(&hook.ThreadId, 0);
				::UnhookWindowsHookEx(hook.hHook);
				hook.CallbackWnd = nullptr;
				hook.TargetHwnd = nullptr;
				hook.Options = HookOptions::None;
				::InterlockedExchangePointer((PVOID*)&hook.hHook, nullptr);
				removed = true;
			}
		}
	}
	return removed;
}
