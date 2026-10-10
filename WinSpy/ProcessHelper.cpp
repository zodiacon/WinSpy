#include "pch.h"
#include "ProcessHelper.h"
#include "WindowHelper.h"
#include <TlHelp32.h>
#include <unordered_set>

CString ProcessHelper::GetProcessImageName(DWORD pid, bool fullPath) {
	if (_names.empty())
		EnumProcesses();

	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	CString result;
	if (hProcess) {
		WCHAR name[MAX_PATH];
		DWORD size = _countof(name);
		if (::QueryFullProcessImageName(hProcess, 0, name, &size)) {
			auto bs = fullPath ? nullptr : ::wcsrchr(name, L'\\');
			result = bs ? bs + 1 : name;
		}
		::CloseHandle(hProcess);
	}
	// the process can't be opened (or its path can't be read): the name it had when the processes were listed, if any
	if (result.IsEmpty())
		if (auto it = _names.find(pid); it != _names.end())
			return it->second;

	return result;
}

ProcessesInfo ProcessHelper::EnumProcessesAndThreads(EnumProcessesOptions options) {
	ProcessesInfo info;
	auto& processes = info.Processes;
	processes.reserve(512);

	auto hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return info;
	CHandle snapshot(hSnapshot);

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
			WCHAR path[MAX_PATH];
			DWORD size = _countof(path);
			if (::QueryFullProcessImageName(hProcess, 0, path, &size))
				pi.FullPath = path;
			::CloseHandle(hProcess);
		}
		processes.push_back(pi);
		processMap.insert({ pi.ProcessId, (int)processes.size() - 1 });
	}

	std::unordered_set<DWORD> msgOnly;
	if ((options & EnumProcessesOptions::IncludeMessageOnly) == EnumProcessesOptions::IncludeMessageOnly) {
		HWND hWnd = nullptr;
		for (;;) {
			hWnd = ::FindWindowEx(HWND_MESSAGE, hWnd, nullptr, nullptr);
			if (!hWnd)
				break;
			auto tid = ::GetWindowThreadProcessId(hWnd, nullptr);
			msgOnly.insert(tid);
			if (auto it = info.MessageOnly.find(tid); it != info.MessageOnly.end()) {
				it->second.push_back(hWnd);
			}
			else {
				info.MessageOnly.insert({ tid, { hWnd } });
			}
		}
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
			if (!WindowHelper::ThreadHasWindows(te.th32ThreadID) && !msgOnly.contains(te.th32ThreadID))
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

	return info;
}

void ProcessHelper::ShowProcessProperties(ProcessInfo const& pi) {
}

void ProcessHelper::EnumProcesses() {
	auto hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	ATLASSERT(hSnapshot != INVALID_HANDLE_VALUE);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		_names.insert({ 0, L"(Idle)" });
		return;
	}
	CHandle snapshot(hSnapshot);

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);
	::Process32First(hSnapshot, &pe);
	_names.reserve(512);

	while (::Process32Next(hSnapshot, &pe)) {
		_names.insert({ pe.th32ProcessID, pe.szExeFile });
	}
}

