#include "pch.h"
#include "ProcessHelper.h"
#include "WindowHelper.h"
#include <TlHelp32.h>
#include <unordered_set>
#include "FormatHelper.h"
#include "PropertiesDlg.h"

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
		pi.Accessible = hProcess != nullptr;
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

namespace {
	CString FormatTime(FILETIME const& ft) {
		SYSTEMTIME st, local;
		if (!::FileTimeToSystemTime(&ft, &st) || !::SystemTimeToTzSpecificLocalTime(nullptr, &st, &local))
			return L"";
		CString text;
		text.Format(L"%04d-%02d-%02d %02d:%02d:%02d", local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
		return text;
	}

	CString FormatWindow(HWND hWnd) {
		if (!hWnd)
			return L"(None)";
		return FormatHelper::FormatHWndOrNone(hWnd) + L" " + WindowHelper::GetWindowClassAndTitle(hWnd);
	}

	DWORD GetParentProcessId(DWORD pid) {
		auto hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE)
			return 0;
		CHandle snapshot(hSnapshot);
		PROCESSENTRY32 pe{ sizeof(pe) };
		for (auto ok = ::Process32First(hSnapshot, &pe); ok; ok = ::Process32Next(hSnapshot, &pe))
			if (pe.th32ProcessID == pid)
				return pe.th32ParentProcessID;
		return 0;
	}

	CString QueryCommandLine(HANDLE hProcess) {
		// ProcessCommandLineInformation (Windows 8.1 and later): a UNICODE_STRING followed by the characters
		using NtQueryInformationProcessFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
		static auto query = (NtQueryInformationProcessFn)::GetProcAddress(::GetModuleHandle(L"ntdll"), "NtQueryInformationProcess");
		if (!query)
			return L"";

		ULONG size = 0;
		query(hProcess, 60, nullptr, 0, &size);
		if (size == 0)
			return L"";
		auto buffer = std::make_unique<BYTE[]>(size);
		if (query(hProcess, 60, buffer.get(), size, &size) < 0)
			return L"";

		struct UnicodeString {
			USHORT Length, MaximumLength;
			PWSTR Buffer;
		};
		auto us = reinterpret_cast<UnicodeString*>(buffer.get());
		return CString(us->Buffer, us->Length / sizeof(WCHAR));
	}

	CString IntegrityLevel(HANDLE hToken) {
		BYTE buffer[256];
		DWORD len;
		if (!::GetTokenInformation(hToken, TokenIntegrityLevel, buffer, sizeof(buffer), &len))
			return L"";
		auto sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer)->Label.Sid;
		auto rid = *::GetSidSubAuthority(sid, *::GetSidSubAuthorityCount(sid) - 1);
		return rid >= SECURITY_MANDATORY_SYSTEM_RID ? L"System" : rid >= SECURITY_MANDATORY_HIGH_RID ? L"High" :
			rid >= SECURITY_MANDATORY_MEDIUM_RID ? L"Medium" : rid >= SECURITY_MANDATORY_LOW_RID ? L"Low" : L"Untrusted";
	}

	void AddTokenProperties(HANDLE hProcess, PropertyList& props) {
		HANDLE hToken;
		if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
			return;
		CHandle token(hToken);

		BYTE buffer[256];
		DWORD len;
		if (::GetTokenInformation(hToken, TokenUser, buffer, sizeof(buffer), &len)) {
			WCHAR name[128], domain[128];
			DWORD nameSize = _countof(name), domainSize = _countof(domain);
			SID_NAME_USE use;
			if (::LookupAccountSid(nullptr, reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, name, &nameSize, domain, &domainSize, &use))
				props.push_back({ L"User", CString(domain) + L"\\" + name });
		}

		if (auto level = IntegrityLevel(hToken); !level.IsEmpty())
			props.push_back({ L"Integrity Level", level });

		TOKEN_ELEVATION elevation;
		if (::GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &len))
			props.push_back({ L"Elevated", elevation.TokenIsElevated ? L"Yes" : L"No" });
	}
}

PropertyList ProcessHelper::GetProcessProperties(ProcessInfo const& pi) {
	PropertyList props;
	CString text;

	props.push_back({ L"Name", pi.ProcessName });
	text.Format(L"%u (0x%X)", pi.ProcessId, pi.ProcessId);
	props.push_back({ L"Process ID", text });
	props.push_back({ L"Path", pi.FullPath.IsEmpty() ? GetProcessImageName(pi.ProcessId, true) : pi.FullPath });

	if (auto parent = GetParentProcessId(pi.ProcessId)) {
		text.Format(L"%s (%u)", (PCWSTR)GetProcessImageName(parent), parent);
		props.push_back({ L"Parent", text });
	}
	DWORD session;
	if (::ProcessIdToSessionId(pi.ProcessId, &session)) {
		text.Format(L"%u", session);
		props.push_back({ L"Session", text });
	}

	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.ProcessId);
	if (hProcess) {
		CHandle process(hProcess);
		props.push_back({ L"Command Line", QueryCommandLine(hProcess) });
		FILETIME created, exited, kernel, user;
		if (::GetProcessTimes(hProcess, &created, &exited, &kernel, &user))
			props.push_back({ L"Started", FormatTime(created) });
		BOOL wow64;
		if (::IsWow64Process(hProcess, &wow64))
			props.push_back({ L"Bitness", wow64 ? L"32-bit" : L"64-bit" });
		AddTokenProperties(hProcess, props);
	}
	else {
		props.push_back({ L"Details", L"Access denied" });
	}

	// (known when the processes were listed with their UI threads)
	if (!pi.Threads.empty()) {
		int windows = 0;
		for (auto tid : pi.Threads)
			::EnumThreadWindows(tid, [](auto, auto p) { (*(int*)p)++; return TRUE; }, (LPARAM)&windows);
		text.Format(L"%u", (DWORD)pi.Threads.size());
		props.push_back({ L"UI Threads", text });
		text.Format(L"%d", windows);
		props.push_back({ L"Top-Level Windows", text });
	}
	return props;
}

void ProcessHelper::ShowProcessProperties(ProcessInfo const& pi, HWND hParent) {
	CString title;
	title.Format(L"Process %s (%u)", (PCWSTR)pi.ProcessName, pi.ProcessId);
	CPropertiesDlg dlg(title, GetProcessProperties(pi));
	dlg.DoModal(hParent);
}

PropertyList ProcessHelper::GetThreadProperties(DWORD tid) {
	PropertyList props;
	CString text;

	text.Format(L"%u (0x%X)", tid, tid);
	props.push_back({ L"Thread ID", text });

	auto hThread = ::OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
	if (hThread) {
		CHandle thread(hThread);
		auto pid = ::GetProcessIdOfThread(hThread);
		text.Format(L"%s (%u)", (PCWSTR)GetProcessImageName(pid), pid);
		props.push_back({ L"Process", text });

		PWSTR description = nullptr;
		if (SUCCEEDED(::GetThreadDescription(hThread, &description)) && description) {
			if (*description)
				props.push_back({ L"Description", description });
			::LocalFree(description);
		}

		FILETIME created, exited, kernel, user;
		if (::GetThreadTimes(hThread, &created, &exited, &kernel, &user))
			props.push_back({ L"Started", FormatTime(created) });
		text.Format(L"%d", ::GetThreadPriority(hThread));
		props.push_back({ L"Priority", text });
	}

	int windows = 0;
	::EnumThreadWindows(tid, [](auto, auto p) { (*(int*)p)++; return TRUE; }, (LPARAM)&windows);
	text.Format(L"%d", windows);
	props.push_back({ L"Top-Level Windows", text });

	text.Format(L"0x%p", ::GetKeyboardLayout(tid));
	props.push_back({ L"Keyboard Layout", text });

	GUITHREADINFO gti{ sizeof(gti) };
	if (::GetGUIThreadInfo(tid, &gti)) {
		props.push_back({ L"Active Window", FormatWindow(gti.hwndActive) });
		props.push_back({ L"Focus Window", FormatWindow(gti.hwndFocus) });
		props.push_back({ L"Capture Window", FormatWindow(gti.hwndCapture) });
		props.push_back({ L"Menu Owner", FormatWindow(gti.hwndMenuOwner) });
		props.push_back({ L"Move/Size Window", FormatWindow(gti.hwndMoveSize) });
		props.push_back({ L"Caret Window", FormatWindow(gti.hwndCaret) });
		if (gti.hwndCaret)
			props.push_back({ L"Caret Rect", FormatHelper::RectToString(gti.rcCaret) });

		static const struct { DWORD flag; PCWSTR name; } flags[] = {
			{ GUI_CARETBLINKING, L"Caret Blinking" }, { GUI_INMENUMODE, L"In Menu Mode" }, { GUI_INMOVESIZE, L"In Move/Size" },
			{ GUI_POPUPMENUMODE, L"Popup Menu Mode" }, { GUI_SYSTEMMENUMODE, L"System Menu Mode" },
		};
		CString state;
		for (auto& f : flags)
			if (gti.flags & f.flag)
				state += (state.IsEmpty() ? L"" : L", ") + CString(f.name);
		props.push_back({ L"GUI State", state.IsEmpty() ? CString(L"None") : state });
	}
	return props;
}

void ProcessHelper::ShowThreadProperties(DWORD tid, HWND hParent) {
	CString title;
	title.Format(L"Thread %u", tid);
	CPropertiesDlg dlg(title, GetThreadProperties(tid));
	dlg.DoModal(hParent);
}

CString ProcessHelper::GetIntegrityLevel(DWORD pid) {
	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProcess)
		return L"(Access denied)";
	CHandle process(hProcess);
	HANDLE hToken;
	if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
		return L"(Access denied)";
	CHandle token(hToken);
	return IntegrityLevel(hToken);
}

bool ProcessHelper::Terminate(DWORD pid) {
	auto hProcess = ::OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (!hProcess)
		return false;
	CHandle process(hProcess);
	return ::TerminateProcess(hProcess, 1);
}
