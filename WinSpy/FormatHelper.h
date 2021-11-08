#pragma once

struct FormatHelper {
	static CString FormatHWndOrNone(HWND hWnd);
	static CString RectToString(CRect const& rc);
	static DWORD_PTR ParseHex(CString const& text);
};

