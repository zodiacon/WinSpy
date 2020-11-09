#include "pch.h"
#include "ProcessHelper.h"

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

