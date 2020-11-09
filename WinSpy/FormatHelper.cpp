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

