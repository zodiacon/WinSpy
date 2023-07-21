#include "pch.h"
#include "HookHelper.h"

decltype(AddHook)* pAddHook;
decltype(RemoveHook)* pRemoveHook;

bool HookHelper::InitHookLib() {
    if (!pAddHook) {
        auto hLib = ::LoadLibrary(L"WinSpyHook.dll");
        if (hLib) {
            pAddHook = (decltype(pAddHook))::GetProcAddress(hLib, "AddHook");
            pRemoveHook = (decltype(pRemoveHook))::GetProcAddress(hLib, "RemoveHook");
        }
    }
    return pAddHook != nullptr;
}

bool __stdcall HookHelper::AddHook(DWORD hookType, HookConfig const& config) {
    if (!InitHookLib())
        return false;

    return pAddHook(hookType, config);
}

bool __stdcall HookHelper::RemoveHook(DWORD tid) {
    if (!InitHookLib())
        return false;

    return pRemoveHook(tid);
}
