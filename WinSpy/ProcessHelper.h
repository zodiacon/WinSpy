#pragma once

struct ProcessInfo {
	DWORD ProcessId;
	std::vector<DWORD> Threads;
	CString ProcessName;
	CString FullPath;
};

enum class EnumProcessesOptions {
	None = 0,
	UIThreadsOnly = 1,
	SkipProcessesWithNoUI = 2,
};
DEFINE_ENUM_FLAG_OPERATORS(EnumProcessesOptions);

struct ProcessHelper {
	static CString GetProcessImageName(DWORD pid, bool fullPath = false);
	static std::vector<ProcessInfo> EnumProcessesAndThreads(EnumProcessesOptions options);
};

