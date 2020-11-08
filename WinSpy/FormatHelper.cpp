#include "pch.h"
#include "FormatHelper.h"

CString FormatHelper::FormatHWndOrNone(HWND hWnd) {
	CString text;
	if (hWnd)
		text.Format(L"0x%zX", hWnd);
	else
		text = L"(None)";
	return text;
}

CString FormatHelper::GetProcessImageName(DWORD pid) {
	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	CString result;
	if (hProcess) {
		WCHAR name[MAX_PATH];
		DWORD size = _countof(name);
		if (::QueryFullProcessImageName(hProcess, 0, name, &size))
			result = ::wcsrchr(name, L'\\') + 1;
		::CloseHandle(hProcess);
	}
	return result;
}
