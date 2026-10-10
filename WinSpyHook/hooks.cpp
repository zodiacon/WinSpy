#include "pch.h"
#include "hooks.h"

// per hook type; a process capture takes one for each of its UI threads
const unsigned MaxHooks = 64;

struct HookEntry {
	HHOOK hHook;
	HWND CallbackWnd;
	LONG volatile ThreadId;
	HWND TargetHwnd;
	HookOptions Options;
	BYTE Filter[FilterLowMessages / 8];		// see SetHookFilter
	BOOL SendHighMessages;
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

//
// the hooks are set on specific threads, so the current thread is the one being watched
//
static bool Passes(HookEntry const& hook, UINT msg) {
	if (msg < FilterLowMessages)
		return (hook.Filter[msg / 8] >> (msg % 8)) & 1;
	return hook.SendHighMessages;
}

static bool IsWanted(int type, HWND hWnd, UINT msg) {
	auto tid = ::GetCurrentThreadId();
	for (auto& hook : g_HookMap[type])
		if ((DWORD)hook.ThreadId == tid && Passes(hook, msg) && IsNotify(hWnd, hook))
			return true;
	return false;
}

static void Send(int type, HWND hWnd, UINT msg, void const* data, DWORD size) {
	auto tid = ::GetCurrentThreadId();
	for (auto& hook : g_HookMap[type]) {
		if ((DWORD)hook.ThreadId == tid && Passes(hook, msg) && IsNotify(hWnd, hook)) {
			COPYDATASTRUCT cds{ 0, size, const_cast<void*>(data) };
			// this runs on the target's thread: don't let a stalled WinSpy hang it
			::SendMessageTimeout(hook.CallbackWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
				SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 200, nullptr);
		}
	}
}

LRESULT WINAPI HookFunc3(int code, WPARAM wParam, LPARAM lParam) {
	//
	// only messages that are taken off the queue; peeks (PM_NOREMOVE) would show the same message again
	//
	if (code == HC_ACTION && (wParam & PM_REMOVE)) {
		auto msg = reinterpret_cast<MSG*>(lParam);
		if (IsWanted(0, msg->hwnd, msg->message)) {
			GetMessageData data;
			data.HookType = WH_GETMESSAGE;
			data.wParam = wParam;
			data.Msg = *msg;
			Send(0, msg->hwnd, msg->message, &data, sizeof(data));
		}
	}
	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

//
// copies a string of the window's character set as UTF-16; count < 0: terminated
//
static DWORD CopyString(HWND hWnd, void const* text, int count, WCHAR* dest, DWORD capacity) {
	if (!text || IS_INTRESOURCE(text))
		return 0;

	if (::IsWindowUnicode(hWnd)) {
		auto s = static_cast<PCWSTR>(text);
		DWORD n = 0;
		for (; n < capacity && (count < 0 ? s[n] != 0 : n < (DWORD)count); n++)
			dest[n] = s[n];
		return n;
	}
	auto s = static_cast<PCSTR>(text);
	int len = 0;
	while ((DWORD)len < capacity && (count < 0 ? s[len] != 0 : len < count))
		len++;
	return len ? ::MultiByteToWideChar(CP_ACP, 0, s, len, dest, capacity) : 0;
}

static void SetExtra(CallWndProcData& data, ExtraDataType type, void const* p, DWORD size) {
	data.ExtraType = type;
	data.ExtraSize = std::min<DWORD>(size, MaxExtraData);
	memcpy(data.Extra, p, data.ExtraSize);
}

static void CopyExtraData(CallWndProcData& data, bool returned) {
	auto lp = data.lParam;
	if (!lp)
		return;

	switch (data.Message) {
		case WM_SETTEXT:
		case WM_GETTEXT:
		{
			if (data.Message == WM_GETTEXT && !returned)
				break;		// nothing in the buffer yet
			auto chars = CopyString(data.hWnd, (void const*)lp, data.Message == WM_GETTEXT ? (int)data.Result : -1,
				(WCHAR*)data.Extra, MaxExtraData / sizeof(WCHAR));
			data.ExtraType = ExtraDataType::Text;
			data.ExtraSize = chars * sizeof(WCHAR);
			break;
		}

		case WM_WINDOWPOSCHANGING:
		case WM_WINDOWPOSCHANGED:
			SetExtra(data, ExtraDataType::WindowPos, (void const*)lp, sizeof(WINDOWPOS));
			break;

		case WM_NOTIFY:
			SetExtra(data, ExtraDataType::Notify, (void const*)lp, sizeof(NMHDR));
			break;

		case WM_STYLECHANGING:
		case WM_STYLECHANGED:
			SetExtra(data, ExtraDataType::Style, (void const*)lp, sizeof(STYLESTRUCT));
			break;

		case WM_GETMINMAXINFO:
			SetExtra(data, ExtraDataType::MinMaxInfo, (void const*)lp, sizeof(MINMAXINFO));
			break;

		case WM_MOVING:
		case WM_SIZING:
			SetExtra(data, ExtraDataType::Rect, (void const*)lp, sizeof(RECT));
			break;

		case WM_NCCALCSIZE:
			// the first rectangle: the window's (sent), then its client area (returned)
			SetExtra(data, ExtraDataType::Rect, data.wParam ? (void const*)&((NCCALCSIZE_PARAMS*)lp)->rgrc[0] : (void const*)lp, sizeof(RECT));
			break;

		case WM_CREATE:
		case WM_NCCREATE:
		{
			// the A and W structures differ only in the strings' types
			auto cs = (CREATESTRUCTW const*)lp;
			CreateData cd{ cs->x, cs->y, cs->cx, cs->cy, cs->style, cs->dwExStyle };
			// a dialog's name can be a resource ID (0xFFFF, ID)
			if (cs->lpszName && !IS_INTRESOURCE(cs->lpszName) && *(WCHAR const*)cs->lpszName != 0xFFFF)
				CopyString(data.hWnd, cs->lpszName, -1, cd.Name, _countof(cd.Name) - 1);
			SetExtra(data, ExtraDataType::Create, &cd, sizeof(cd));
			break;
		}
	}
}

//
// the pointers come from the target's code: a bad one must not crash it
//
static void CopyExtra(CallWndProcData& data, bool returned) {
	__try {
		CopyExtraData(data, returned);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		data.ExtraType = ExtraDataType::None;
		data.ExtraSize = 0;
	}
}

static void SendCallWndProc(int type, DWORD hookType, HWND hWnd, UINT message, WPARAM wp, LPARAM lp, LRESULT result, BOOL fromOtherThread) {
	CallWndProcData data;
	data.HookType = hookType;
	data.hWnd = hWnd;
	data.Message = message;
	data.wParam = wp;
	data.lParam = lp;
	data.Result = result;
	data.Time = ::GetTickCount();
	data.FromOtherThread = fromOtherThread;
	data.ExtraType = ExtraDataType::None;
	data.ExtraSize = 0;
	CopyExtra(data, hookType == WH_CALLWNDPROCRET);
	Send(type, hWnd, message, &data, (DWORD)offsetof(CallWndProcData, Extra) + data.ExtraSize);
}

LRESULT WINAPI HookFunc4(int code, WPARAM wParam, LPARAM lParam) {
	if (code == HC_ACTION) {
		auto cwp = reinterpret_cast<CWPSTRUCT*>(lParam);
		if (IsWanted(1, cwp->hwnd, cwp->message))
			SendCallWndProc(1, WH_CALLWNDPROC, cwp->hwnd, cwp->message, cwp->wParam, cwp->lParam, 0, wParam != 0);
	}
	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT WINAPI HookFunc12(int code, WPARAM wParam, LPARAM lParam) {
	if (code == HC_ACTION) {
		auto cwp = reinterpret_cast<CWPRETSTRUCT*>(lParam);
		if (IsWanted(2, cwp->hwnd, cwp->message))
			SendCallWndProc(2, WH_CALLWNDPROCRET, cwp->hwnd, cwp->message, cwp->wParam, cwp->lParam, cwp->lResult, wParam != 0);
	}
	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

static int HookTypeIndex(DWORD hookType) {
	switch (hookType) {
		case WH_GETMESSAGE: return 0;
		case WH_CALLWNDPROC: return 1;
		case WH_CALLWNDPROCRET: return 2;
	}
	return -1;
}

bool WINAPI AddHook(DWORD hookType, HookConfig const& config) {
	auto index = HookTypeIndex(hookType);
	ATLASSERT(index >= 0);
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
	// everything, until a filter is set
	memset(slot->Filter, 0xFF, sizeof(slot->Filter));
	slot->SendHighMessages = TRUE;
	::InterlockedExchangePointer((PVOID*)&slot->hHook, hHook);
	::InterlockedExchange(&slot->ThreadId, (LONG)config.ThreadId);
	return true;
}

bool WINAPI SetHookFilter(HWND callbackWnd, DWORD hookType, BYTE const* lowMessages, BOOL highMessages) {
	auto index = HookTypeIndex(hookType);
	if (index < 0)
		return false;

	bool found = false;
	for (auto& hook : g_HookMap[index]) {
		if (hook.ThreadId && hook.CallbackWnd == callbackWnd) {
			// byte-sized writes: a hook procedure reading meanwhile sees either the old or the new bit
			memcpy(hook.Filter, lowMessages, sizeof(hook.Filter));
			hook.SendHighMessages = highMessages;
			found = true;
		}
	}
	return found;
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
