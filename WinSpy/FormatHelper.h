#pragma once

// named values, as properties lists show them
using PropertyList = std::vector<std::pair<CString, CString>>;

struct FormatHelper {
	static CString FormatHWndOrNone(HWND hWnd);
	static CString RectToString(CRect const& rc);
	static DWORD_PTR ParseHex(CString const& text);
	static CString FormatPoint(POINT const& pt);
};

