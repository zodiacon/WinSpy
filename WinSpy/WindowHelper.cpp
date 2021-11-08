#include "pch.h"
#include "resource.h"
#include "WindowHelper.h"
#include "ProcessHelper.h"
#include "FormatHelper.h"
#include "WindowGeneralPage.h"

CString WindowHelper::WindowStyleToString(HWND hWnd) {
	auto style = CWindow(hWnd).GetStyle();
	CString text;

	auto const [styles, count] = GetWindowStyleArray();
	for (int i = 0; i < count; i++) {
		auto& item = styles[i];
		if ((style & item.Value) == item.Value)
			text += CString(item.Text) += L", ";
	}
	if (text.IsEmpty())
		text = L"OVERLAPPED, ";

	return L"(WS_) " + text.Left(text.GetLength() - 2);
}

CString WindowHelper::ClassStyleToString(HWND hWnd) {
	auto style = ::GetClassLongPtr(hWnd, GCL_STYLE);
	CString text;

	auto const [styles, count] = GetWindowStyleArray();
	for (int i = 0; i < count; i++) {
		auto& item = styles[i];
		if ((style & item.Value) == item.Value)
			text += CString(item.Text) += L", ";
	}
	if (text.IsEmpty())
		return L"";

	return L"(CS_) " + text.Left(text.GetLength() - 2);
}

CString WindowHelper::WindowExtendedStyleToString(HWND hWnd) {
	auto style = CWindow(hWnd).GetExStyle();
	CString text;

	auto const [styles, count] = GetWindowStyleArray();
	for (int i = 0; i < count; i++) {
		auto& item = styles[i];
		if ((style & item.Value) == item.Value)
			text += CString(item.Text) += L", ";
	}

	if (text.IsEmpty())
		text = L"LEFT, RIGHTSCROLLBAR, ";

	return L"(WS_EX_) " + text.Left(text.GetLength() - 2);
}

CString WindowHelper::WindowRectToString(HWND hWnd) {
	CWindow win(hWnd);
	CRect rc;
	win.GetWindowRect(&rc);
	return FormatHelper::RectToString(rc);
}

CString WindowHelper::GetWindowClassName(HWND hWnd) {
	WCHAR name[128];
	::GetClassName(hWnd, name, _countof(name));
	return name;
}

CString WindowHelper::GetWindowText(HWND hWnd) {
	CString text;
	CWindow(hWnd).GetWindowText(text);
	return text;
}

HICON WindowHelper::GetWindowOrProcessIcon(HWND hWnd) {
	auto hIcon = GetWindowIcon(hWnd);
	if (!hIcon) {
		DWORD pid = 0;
		::GetWindowThreadProcessId(hWnd, &pid);
		if (pid) {
			::ExtractIconEx(ProcessHelper::GetProcessImageName(pid, true), 0, nullptr, &hIcon, 1);
		}
	}

	return hIcon;
}

bool WindowHelper::Flash(HWND hWnd) {
	FLASHWINFO info = { sizeof(info) };
	info.dwFlags = FLASHW_CAPTION;
	info.uCount = 3;
	info.hwnd = hWnd;
	return ::FlashWindowEx(&info);
}

void WindowHelper::HighlightBorder(HWND hWnd, bool highlight) {
	CRect rc;
	::GetWindowRect(hWnd, &rc);
	rc.OffsetRect(-rc.left, -rc.top);

	if (!highlight) {
		CRgn rgn1;
		rc.InflateRect(2, 2);
		rgn1.CreateRectRgnIndirect(&rc);
		rc.DeflateRect(6, 6);
		CRgn rgn2;
		rgn2.CreateRectRgnIndirect(&rc);
		CRgn rgn;
		rgn.CreateRectRgn(0, 0, 1, 1);
		rgn.CombineRgn(rgn1, rgn2, RGN_DIFF);
		::RedrawWindow(hWnd, nullptr, rgn, RDW_INTERNALPAINT | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
		return;
	}

	CWindowDC dc(hWnd);
	int save = dc.SaveDC();
	CPen pen;
	pen.CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
	dc.SelectStockBrush(NULL_BRUSH);
	dc.SelectPen(pen);
	dc.Rectangle(&rc);
	dc.RestoreDC(save);
}

HICON WindowHelper::GetWindowIcon(HWND hWnd) {
	HICON hIcon{ nullptr };
	::SendMessageTimeout(hWnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 100, (DWORD_PTR*)&hIcon);
	if (!hIcon) {
		hIcon = (HICON)::GetClassLongPtr(hWnd, GCLP_HICONSM);
	}
	return hIcon;
}

CString WindowHelper::GetWindowClassAndTitle(HWND hWnd) {
	if (hWnd == nullptr || !::IsWindow(hWnd))
		return L"";

	WCHAR className[64];
	CString text;
	if (::GetClassName(hWnd, className, _countof(className)))
		text.Format(L"[%s]", className);
	CString title;
	CWindow(hWnd).GetWindowText(title);
	if (!title.IsEmpty())
		text.Format(L"%s (%s)", (PCWSTR)text, (PCWSTR)title);
	return text;
}

std::unordered_map<HWND, int>& WindowHelper::GetIconMap() {
	static std::unordered_map<HWND, int> s_IconMap;
	return s_IconMap;
}

CImageList& WindowHelper::GetImageList() {
	static CImageList images;
	if (!images) {
		images.Create(16, 16, ILC_COLOR32, 64, 16);
		images.AddIcon(AtlLoadIconImage(IDI_WINDOW, 0, 16, 16));
	}
	return images;
}

WindowItem WindowHelper::GetWindowInfo(HWND hWnd) {
	WindowItem wi;
	wi.hWnd = hWnd;
	wi.ThreadId = ::GetWindowThreadProcessId(hWnd, &wi.ProcessId);
	wi.ProcessName = ProcessHelper::GetProcessImageName(wi.ProcessId);

	return wi;
}

bool WindowHelper::ThreadHasWindows(DWORD tid) {
	bool hasWindows = false;
	::EnumThreadWindows(tid, [](auto, auto param) {
		*reinterpret_cast<bool*>(param) = true;
		return FALSE;
		}, reinterpret_cast<LPARAM>(&hasWindows));
	return hasWindows;
}

int WindowHelper::ShowWindowProperties(HWND hWnd) {
	CString text;
	text.Format(L"Window 0x%X Proerties", PtrToUlong(hWnd));
	CPropertySheet sheet((PCWSTR)text);
	(sheet.m_psh.dwFlags |= PSH_NOAPPLYNOW) &= ~PSH_HASHELP;
	CWindowGeneralPage general(hWnd);
	sheet.AddPage(general);
	sheet.DoModal();

	return 0;
}

std::pair<StyleItem const*, int> WindowHelper::GetWindowStyleArray() {
	static const StyleItem styles[] = {
		{ WS_POPUP			, L"POPUP" },
		{ WS_CHILD			, L"CHILD" },
		{ WS_MINIMIZE		, L"MINIMIZE" },
		{ WS_VISIBLE		, L"VISIBLE" },
		{ WS_DISABLED		, L"DISABLED" },
		{ WS_CLIPSIBLINGS	, L"CLIPSIBLINGS" },
		{ WS_CLIPCHILDREN	, L"CLIPCHILDREN" },
		{ WS_MAXIMIZE		, L"MAXIMIZE" },
		{ WS_BORDER			, L"BORDER" },
		{ WS_DLGFRAME		, L"DLGFRAME" },
		{ WS_VSCROLL		, L"VSCROLL" },
		{ WS_HSCROLL		, L"HSCROLL" },
		{ WS_SYSMENU		, L"SYSMENU" },
		{ WS_THICKFRAME		, L"THICKFRAME" },
		{ WS_MINIMIZEBOX	, L"MINIMIZEBOX" },
		{ WS_MAXIMIZEBOX	, L"MAXIMIZEBOX" },
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetWindowStyleExArray() {
	static const StyleItem styles[] = {
		{ WS_EX_DLGMODALFRAME		, L"DLGMODALFRAME" },
		{ WS_EX_NOPARENTNOTIFY		, L"NOPARENTNOTIFY" },
		{ WS_EX_TOPMOST				, L"TOPMOST" },
		{ WS_EX_ACCEPTFILES			, L"ACCEPTFILES" },
		{ WS_EX_TRANSPARENT			, L"TRANSPARENT" },
		{ WS_EX_MDICHILD			, L"MDICHILD" },
		{ WS_EX_TOOLWINDOW			, L"TOOLWINDOW" },
		{ WS_EX_WINDOWEDGE			, L"WINDOWEDGE" },
		{ WS_EX_CLIENTEDGE			, L"CLIENTEDGE" },
		{ WS_EX_CONTEXTHELP			, L"CONTEXTHELP" },
		{ WS_EX_RIGHT				, L"RIGHT" },
		{ WS_EX_RTLREADING			, L"RTLREADING" },
//		{ WS_EX_LTRREADING			, L"LTRREADING" },
		{ WS_EX_LEFTSCROLLBAR		, L"LEFTSCROLLBAR" },
		{ WS_EX_CONTROLPARENT		, L"CONTROLPARENT" },
		{ WS_EX_STATICEDGE			, L"STATICEDGE" },
		{ WS_EX_APPWINDOW			, L"APPWINDOW" },
		{ WS_EX_LAYERED				, L"LAYERED" },
		{ WS_EX_NOINHERITLAYOUT		, L"NOINHERITLAYOUT" },
		{ WS_EX_NOREDIRECTIONBITMAP , L"NOREDIRECTIONBITMAP" },
		{ WS_EX_LAYOUTRTL			, L"LAYOUTRTL" },
		{ WS_EX_COMPOSITED			, L"COMPOSITED" },
		{ WS_EX_NOACTIVATE			, L"NOACTIVATE" },
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetClassStyleArray() {
	static const StyleItem styles[] = {
		{ CS_HREDRAW			, L"HREDRAW" },
		{ CS_VREDRAW			, L"VREDRAW" },
		{ CS_DBLCLKS			, L"DBLCLKS" },
		{ CS_OWNDC				, L"OWNDC" },
		{ CS_CLASSDC			, L"CLASSDC" },
		{ CS_PARENTDC			, L"PARENTDC" },
		{ CS_SAVEBITS			, L"SAVEBITS" },
		{ CS_GLOBALCLASS		, L"GLOBALCLASS" },
		{ CS_BYTEALIGNCLIENT	, L"BYTEALIGNCLIENT" },
		{ CS_BYTEALIGNWINDOW	, L"BYTEALIGWINDOW" },
		{ CS_IME				, L"IME" },
		{ CS_DROPSHADOW			, L"DROPSHADOW" },
	};
	return std::make_pair(styles, (int)_countof(styles));
}

