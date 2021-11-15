#include "pch.h"
#include "ProcessHelper.h"
#include "WindowHelper.h"
#include <TlHelp32.h>

CString ProcessHelper::GetProcessImageName(DWORD pid, bool fullPath) {
	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	CString result;
	if (hProcess) {
		WCHAR name[MAX_PATH];
		DWORD size = _countof(name);
		if (::QueryFullProcessImageName(hProcess, 0, name, &size)) {
			result = fullPath ? name : ::wcsrchr(name, L'\\') + 1;
		}
		::CloseHandle(hProcess);
	}
	return result;
}

std::vector<ProcessInfo> ProcessHelper::EnumProcessesAndThreads(EnumProcessesOptions options) {
	std::vector<ProcessInfo> processes;
	processes.reserve(512);

	auto hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return processes;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);

	std::unordered_map<DWORD, int> processMap;
	processMap.reserve(512);

	::Process32First(hSnapshot, &pe);

	while (::Process32Next(hSnapshot, &pe)) {
		ProcessInfo pi;
		pi.ProcessId = pe.th32ProcessID;
		pi.Threads.reserve(pe.cntThreads);
		pi.ProcessName = pe.szExeFile;
		auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.ProcessId);
		if (hProcess) {
			DWORD size = MAX_PATH;
			::QueryFullProcessImageName(hProcess, 0, pi.FullPath.GetBufferSetLength(size), &size);
			::CloseHandle(hProcess);
		}
		processes.push_back(pi);
		processMap.insert({ pi.ProcessId, (int)processes.size() - 1 });
	}

	THREADENTRY32 te;
	te.dwSize = sizeof(te);

	::Thread32First(hSnapshot, &te);

	do {
		//
		// skip idle process
		//
		if (te.th32OwnerProcessID == 0)
			continue;

		auto& pi = processes[processMap[te.th32OwnerProcessID]];
		if ((options & EnumProcessesOptions::UIThreadsOnly) == EnumProcessesOptions::UIThreadsOnly) {
			if(!WindowHelper::ThreadHasWindows(te.th32ThreadID))
				continue;
		}
		pi.Threads.push_back(te.th32ThreadID);
	} while (::Thread32Next(hSnapshot, &te));

	if ((options & EnumProcessesOptions::SkipProcessesWithNoUI) == EnumProcessesOptions::SkipProcessesWithNoUI) {
		for (int i = 0; i < (int)processes.size(); i++) {
			if (processes[i].Threads.empty()) {
				processes.erase(processes.begin() + i);
				i--;
			}
		}
	}

	return processes;
}

void ProcessHelper::ShowProcessProperties(ProcessInfo const& pi) {
}

