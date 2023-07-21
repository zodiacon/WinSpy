#pragma once

#include "hooks.h"

struct HookHelper abstract final {
	static bool InitHookLib();
	static bool WINAPI AddHook(DWORD hookType, HookConfig const& config);
	static bool WINAPI RemoveHook(DWORD tid);

};

