#pragma once

struct SecurityHelper final {
	static bool IsRunningElevated();
	static bool RunElevated();
	static bool EnablePrivilege(PCWSTR privName, bool enable = true);
};
