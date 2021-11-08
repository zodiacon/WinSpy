#include "pch.h"
#include "FormatHelper.h"
#include <sstream>

CString FormatHelper::FormatHWndOrNone(HWND hWnd) {
	CString text;
	if (hWnd)
		text.Format(L"0x%zX", (DWORD_PTR)hWnd);
	else
		text = L"(None)";
	return text;
}

CString FormatHelper::RectToString(CRect const& rc) {
	CString text;
	text.Format(L"(%d,%d)-(%d,%d) [%d x %d]", rc.left, rc.top, rc.right, rc.bottom, rc.Width(), rc.Height());
	return text;
}

DWORD_PTR FormatHelper::ParseHex(CString const& text) {
	std::wstringstream ss;
	ss << std::hex;
	if (text.Left(2).CompareNoCase(L"0x") == 0)
		ss << (PCWSTR)text.Mid(2);
	else
		ss << (PCWSTR)text;
	DWORD_PTR value;
	ss >> value;
	return value;
}
