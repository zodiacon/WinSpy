#include "pch.h"
#include "HookManager.h"

__declspec(dllimport) HHOOK g_hooks[WH_MAX + 1];

bool HookManager::InstallHooks(HWND hWnd) {
    auto hLib = ::LoadLibrary(L"WinSpyHook.Dll");
    if (!hLib)
        return false;

    const struct {
        int HookType;
        PCSTR HookFunc;
    } hooks[] = {
        { WH_CBT, "CBTHook" },
        { WH_CALLWNDPROC, "CallWndProcHook" },
        { WH_GETMESSAGE, "GetMessageHook" },
    };
    auto tid = ::GetWindowThreadProcessId(hWnd, nullptr);

    for (auto& hi : hooks) {
        g_hooks[hi.HookType] = ::SetWindowsHookEx(hi.HookType, (HOOKPROC)::GetProcAddress(hLib, hi.HookFunc), hLib, tid);
    }
    return true;
}

bool HookManager::RemoveHooks() {
    for (auto& hook : g_hooks) {
        if (hook) {
            ::UnhookWindowsHookEx(hook);
            hook = nullptr;
        }
    }
    return true;
}
