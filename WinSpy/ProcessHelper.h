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
	IncludeMessageOnly = 4,
};
DEFINE_ENUM_FLAG_OPERATORS(EnumProcessesOptions);

struct ProcessesInfo {
	std::vector<ProcessInfo> Processes;
	std::unordered_map<DWORD, std::vector<HWND>> MessageOnly;
};

struct ProcessHelper {
	static CString GetProcessImageName(DWORD pid, bool fullPath = false);
	static ProcessesInfo EnumProcessesAndThreads(EnumProcessesOptions options);
	static void ShowProcessProperties(ProcessInfo const& pi);
};

