#pragma once

struct FormatHelper {
	static CString FormatHWndOrNone(HWND hWnd);
	static CString GetProcessImageName(DWORD pid);
};

