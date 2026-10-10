#include "pch.h"
#include "HookHelper.h"

decltype(AddHook)* pAddHook;
decltype(RemoveHook)* pRemoveHook;
decltype(SetHookFilter)* pSetHookFilter;

bool HookHelper::InitHookLib() {
    if (!pAddHook) {
        auto hLib = ::LoadLibrary(L"WinSpyHook.dll");
        if (hLib) {
            pAddHook = (decltype(pAddHook))::GetProcAddress(hLib, "AddHook");
            pRemoveHook = (decltype(pRemoveHook))::GetProcAddress(hLib, "RemoveHook");
            pSetHookFilter = (decltype(pSetHookFilter))::GetProcAddress(hLib, "SetHookFilter");
        }
    }
    return pAddHook != nullptr;
}

bool __stdcall HookHelper::AddHook(DWORD hookType, HookConfig const& config) {
    if (!InitHookLib())
        return false;

    return pAddHook(hookType, config);
}

bool __stdcall HookHelper::RemoveHook(HWND callbackWnd) {
    if (!InitHookLib())
        return false;

    return pRemoveHook(callbackWnd);
}

bool __stdcall HookHelper::SetHookFilter(HWND callbackWnd, DWORD hookType, BYTE const* lowMessages, BOOL highMessages) {
    if (!InitHookLib() || !pSetHookFilter)
        return false;

    return pSetHookFilter(callbackWnd, hookType, lowMessages, highMessages);
}
