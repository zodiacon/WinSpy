#include "pch.h"
#include "resource.h"
#include "WindowHelper.h"
#include "ProcessHelper.h"
#include "FormatHelper.h"
#include "WindowGeneralPage.h"
#include "WindowWindowsPage.h"
#include "PropertiesDlg.h"
#include "AppSettings.h"
#include <ResizablePropertySheet.h>
#include <IconPropertySheet.h>
#include <ShellScalingApi.h>

#pragma comment(lib, "shcore")

namespace {
	UINT s_Dpi;		// the main window's (0: not set, the system's)

	// the window icons' sources, by their index in the image list (index 0 is the default, from the resources)
	std::vector<HICON>& GetIconSources() {
		static std::vector<HICON> s_Sources;
		return s_Sources;
	}

	// the window properties: resizable, with full color tab icons
	class CWindowPropertiesSheet :
		public CIconPropertySheetImpl<CWindowPropertiesSheet, CResizablePropertySheetImpl<CWindowPropertiesSheet>> {
	public:
		using CIconPropertySheetImpl::CIconPropertySheetImpl;
	};

	// a red frame around a rectangle: a topmost window with a hole (the region), that the mouse goes through;
	// it flashes a few times, then destroys itself
	using CHighlightTraits = CWinTraits<WS_POPUP, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT>;
	class CHighlightFrame : public CWindowImpl<CHighlightFrame, CWindow, CHighlightTraits> {
	public:
		DECLARE_WND_CLASS_EX(L"WinSpyHighlightFrame", 0, -1)

		bool Show(RECT const& rc) {
			CRect rcWindow(rc);
			// as thick as the monitor's DPI makes it
			UINT dpiX = USER_DEFAULT_SCREEN_DPI, dpiY;
			::GetDpiForMonitor(::MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
			int width = ::MulDiv(3, dpiX, USER_DEFAULT_SCREEN_DPI);
			rcWindow.InflateRect(width, width);
			if (!Create(nullptr, rcWindow))
				return false;

			SetLayeredWindowAttributes(m_hWnd, 0, 255, LWA_ALPHA);
			CRgn outer, inner;
			outer.CreateRectRgn(0, 0, rcWindow.Width(), rcWindow.Height());
			inner.CreateRectRgn(width, width, rcWindow.Width() - width, rcWindow.Height() - width);
			outer.CombineRgn(inner, RGN_DIFF);
			SetWindowRgn(outer.Detach(), FALSE);
			ShowWindow(SW_SHOWNOACTIVATE);
			SetTimer(1, 500);
			return true;
		}

		BEGIN_MSG_MAP(CHighlightFrame)
			MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
			MESSAGE_HANDLER(WM_TIMER, OnTimer)
		END_MSG_MAP()

	private:
		void OnFinalMessage(HWND) override {
			delete this;
		}

		LRESULT OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&) {
			CDCHandle dc((HDC)wParam);
			CRect rc;
			GetClientRect(&rc);
			dc.FillSolidRect(&rc, RGB(255, 0, 0));
			return 1;
		}

		LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
			// shown, hidden, shown... three times
			if (++m_Ticks == 6)
				DestroyWindow();
			else
				ShowWindow(m_Ticks % 2 ? SW_HIDE : SW_SHOWNOACTIVATE);
			return 0;
		}

		int m_Ticks{ 0 };
	};
}
#include <dwmapi.h>
#include <Psapi.h>

#pragma comment(lib, "dwmapi.lib")

#define CASE_STR(x) case x: return L#x 
#define PAIR_STR(x) { x, L#x }
#define PAIR_STR2(x, mask) { x, L#x, mask }

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

	auto const [styles, count] = GetClassStyleArray();
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

	auto const [styles, count] = GetWindowStyleExArray();
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
			//
			// the extracted icons are owned here (one per executable), as callers don't destroy the icons they get
			//
			static std::unordered_map<std::wstring, HICON> s_ProcessIcons;
			std::wstring path(ProcessHelper::GetProcessImageName(pid, true));
			if (auto it = s_ProcessIcons.find(path); it != s_ProcessIcons.end())
				return it->second;
			::ExtractIconEx(path.c_str(), 0, nullptr, &hIcon, 1);
			s_ProcessIcons.insert({ std::move(path), hIcon });
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

int WindowHelper::DpiScale(int size) {
	return ::MulDiv(size, GetDpi(), USER_DEFAULT_SCREEN_DPI);
}

UINT WindowHelper::GetDpi() {
	return s_Dpi ? s_Dpi : ::GetDpiForSystem();
}

void WindowHelper::SetDpi(UINT dpi) {
	if (dpi == 0 || dpi == GetDpi()) {
		s_Dpi = dpi;
		return;
	}
	s_Dpi = dpi;

	// setting the size removes the images: they are added again in the same order
	auto& images = GetImageList();
	int count = images.GetImageCount();
	int size = DpiScale(16);
	images.SetIconSize(size, size);
	auto hDefault = AtlLoadIconImage(IDI_WINDOW, 0, size, size);
	images.AddIcon(hDefault);
	auto& sources = GetIconSources();
	for (int i = 1; i < count; i++)
		images.AddIcon(i < (int)sources.size() && sources[i] ? sources[i] : hDefault);
	::DestroyIcon(hDefault);
}

int WindowHelper::AddIcon(HICON hIcon) {
	int index = GetImageList().AddIcon(hIcon);
	if (index > 0) {
		auto& sources = GetIconSources();
		if ((int)sources.size() <= index)
			sources.resize(index + 1);
		sources[index] = ::CopyIcon(hIcon);
	}
	return index;
}

HFONT WindowHelper::GetUIFont() {
	// one for each DPI used (they are not deleted: a control may still have the last one)
	static std::unordered_map<UINT, HFONT> s_Fonts;
	auto dpi = GetDpi();
	if (auto it = s_Fonts.find(dpi); it != s_Fonts.end())
		return it->second;

	NONCLIENTMETRICS ncm{ sizeof(ncm) };
	::SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi);
	auto hFont = ::CreateFontIndirect(&ncm.lfMessageFont);
	s_Fonts[dpi] = hFont;
	return hFont;
}

void WindowHelper::FitToolBarBand(HWND hReBar, HWND hToolBar) {
	CToolBarCtrl tb(hToolBar);
	tb.AutoSize();
	SIZE size;
	tb.GetMaxSize(&size);
	CReBarCtrl rb(hReBar);
	int count = rb.GetBandCount();
	for (int i = 0; i < count; i++) {
		REBARBANDINFO rbbi{ sizeof(rbbi) };
		rbbi.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE;
		rb.GetBandInfo(i, &rbbi);
		if (rbbi.hwndChild != hToolBar)
			continue;
		rbbi.fMask = RBBIM_CHILDSIZE | RBBIM_IDEALSIZE | RBBIM_SIZE;
		rbbi.cxMinChild = rbbi.cx = rbbi.cxIdeal = size.cx;
		rbbi.cyMinChild = size.cy;
		rb.SetBandInfo(i, &rbbi);
	}
}

void WindowHelper::ScaleColumns(HWND hRoot, UINT oldDpi, UINT newDpi) {
	auto scale = [](HWND hWnd, UINT oldDpi, UINT newDpi) {
		WCHAR className[32];
		if (!::GetClassName(hWnd, className, _countof(className)) || _wcsicmp(className, WC_LISTVIEW) != 0)
			return;
		CListViewCtrl list(hWnd);
		int count = list.GetHeader().GetItemCount();
		for (int i = 0; i < count; i++)
			list.SetColumnWidth(i, ::MulDiv(list.GetColumnWidth(i), newDpi, oldDpi));
		};
	struct Params {
		decltype(scale)& Scale;
		UINT OldDpi, NewDpi;
	} params{ scale, oldDpi, newDpi };
	::EnumChildWindows(hRoot, [](auto hWnd, auto p) {
		auto params = reinterpret_cast<Params*>(p);
		params->Scale(hWnd, params->OldDpi, params->NewDpi);
		return TRUE;
		}, reinterpret_cast<LPARAM>(&params));
}

CString WindowHelper::GetColumnsLayout(CListViewCtrl& list) {
	// "96;" then "width:order," for each column, the widths at 96 DPI
	int count = list.GetHeader().GetItemCount();
	std::vector<int> order(count);
	list.GetColumnOrderArray(count, order.data());
	auto dpi = ::GetDpiForWindow(list);
	CString text(L"96;"), item;
	for (int i = 0; i < count; i++) {
		item.Format(L"%d:%d,", ::MulDiv(list.GetColumnWidth(i), USER_DEFAULT_SCREEN_DPI, dpi), order[i]);
		text += item;
	}
	return text;
}

void WindowHelper::SetColumnsLayout(CListViewCtrl& list, CString const& saved) {
	// the widths are at 96 DPI ("96;" first), or (saved by an earlier version) in pixels at the system DPI
	UINT layoutDpi = ::GetDpiForSystem();
	CString layout(saved);
	if (layout.Left(3) == L"96;") {
		layoutDpi = USER_DEFAULT_SCREEN_DPI;
		layout = layout.Mid(3);
	}
	std::vector<int> widths, order;
	int start = 0;
	for (auto token = layout.Tokenize(L",", start); start >= 0; token = layout.Tokenize(L",", start)) {
		int colon = token.Find(L':');
		if (colon < 0)
			return;
		widths.push_back(_wtoi(token.Left(colon)));
		order.push_back(_wtoi(token.Mid(colon + 1)));
	}

	int count = list.GetHeader().GetItemCount();
	if ((int)widths.size() != count)
		return;
	auto sorted = order;
	std::sort(sorted.begin(), sorted.end());
	for (int i = 0; i < count; i++)
		if (sorted[i] != i)
			return;

	auto dpi = ::GetDpiForWindow(list);
	for (int i = 0; i < count; i++)
		list.SetColumnWidth(i, ::MulDiv(widths[i], dpi, layoutDpi));
	list.SetColumnOrderArray(count, order.data());
}

CString WindowHelper::GetDpiAwareness(HWND hWnd) {
	auto context = ::GetWindowDpiAwarenessContext(hWnd);
	if (!context)
		return L"";
	PCWSTR name;
	if (::AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
		name = L"Per-Monitor v2";
	else if (::AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED))
		name = L"Unaware (GDI Scaled)";
	else {
		switch (::GetAwarenessFromDpiAwarenessContext(context)) {
			case DPI_AWARENESS_UNAWARE: name = L"Unaware"; break;
			case DPI_AWARENESS_SYSTEM_AWARE: name = L"System"; break;
			case DPI_AWARENESS_PER_MONITOR_AWARE: name = L"Per-Monitor"; break;
			default: name = L"?"; break;
		}
	}
	CString text;
	text.Format(L"%s (%u)", name, ::GetDpiForWindow(hWnd));
	return text;
}

CString WindowHelper::GetCloakedState(HWND hWnd) {
	// DWMWA_CLOAKED (Windows 8 and later)
	DWORD cloaked = 0;
	if (FAILED(::DwmGetWindowAttribute(hWnd, 14, &cloaked, sizeof(cloaked))))
		return L"";
	switch (cloaked) {
		case 0: return L"No";
		case 1: return L"App";		// DWM_CLOAKED_APP
		case 2: return L"Shell";	// DWM_CLOAKED_SHELL
		case 4: return L"Inherited";	// DWM_CLOAKED_INHERITED
	}
	CString text;
	text.Format(L"0x%X", cloaked);
	return text;
}

void WindowHelper::HighlightWindow(HWND hWnd) {
	::TrySubmitThreadpoolCallback([](auto, auto p) {
		auto hWnd = (HWND)p;
		for (int i = 0; i < 3; i++) {
			HighlightBorder(hWnd);
			::Sleep(500);
			HighlightBorder(hWnd, false);
			::Sleep(500);
		}
		}, hWnd, nullptr);
}

void WindowHelper::HighlightRect(RECT const& rc) {
	auto frame = new CHighlightFrame;
	if (!frame->Show(rc))
		delete frame;
}

void WindowHelper::HighlightBorder(HWND hWnd, bool highlight) {
	// a null window would mean drawing on the whole screen
	if (!::IsWindow(hWnd))
		return;

	if (!highlight) {
		// the whole window: the highlight is in window (not client) coordinates, which a region given to RedrawWindow is not
		::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
		return;
	}

	CRect rc;
	::GetWindowRect(hWnd, &rc);
	rc.OffsetRect(-rc.left, -rc.top);

	CRgn rgn1;
	rc.InflateRect(2, 2);
	rgn1.CreateRectRgnIndirect(&rc);
	rc.DeflateRect(5, 5);
	CRgn rgn2;
	rgn2.CreateRectRgnIndirect(&rc);
	CRgn rgn;
	rgn.CreateRectRgn(0, 0, 1, 1);
	rgn.CombineRgn(rgn1, rgn2, RGN_DIFF);
	CWindowDC dc(hWnd);
	CBrush b;
	b.CreateSolidBrush(RGB(255, 0, 0));
	dc.FillRgn(rgn, b);
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

void WindowHelper::PruneIconMap() {
	std::erase_if(GetIconMap(), [](auto const& item) { return !::IsWindow(item.first); });
}

std::unordered_map<HWND, int>& WindowHelper::GetIconMap() {
	static std::unordered_map<HWND, int> s_IconMap;
	return s_IconMap;
}

CImageList& WindowHelper::GetImageList() {
	static CImageList images;
	if (!images) {
		int size = DpiScale(16);
		images.Create(size, size, ILC_COLOR32, 64, 16);
		images.AddIcon(AtlLoadIconImage(IDI_WINDOW, 0, size, size));
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

std::pair<StyleItem const*, int> WindowHelper::GetControlStyleArray(HWND hWnd, PCWSTR* defaultStyle) {
	static const struct {
		PCWSTR className;
		std::pair<StyleItem const*, int> items;
		PCWSTR def = L"";
	} controls[] = {
		{ WC_LISTVIEW, GetListViewStyleArray(), L"LVS_ALIGNTOP" },
		{ WC_TREEVIEW, GetTreeViewStyleArray() },
		{ WC_TABCONTROL, GetTabCtrlStyleArray(), L"TCS_TABS" },
		{ WC_LISTBOX, GetListBoxStyleArray() },
		{ WC_COMBOBOX, GetComboBoxStyleArray() },
		{ WC_EDIT, GetEditStyleArray(), L"ES_LEFT" },
		{ WC_BUTTON, GetButtonStyleArray(), L"BS_TEXT" },
		{ WC_HEADER, GetHeaderStyleArray(), L"HDS_HORZ" },
		{ WC_STATIC, GetStaticStyleArray(), L"SS_LEFT" },
		{ TOOLTIPS_CLASS, GetToolTipStyleArray() },
		{ STATUSCLASSNAME, GetStatusBarStyleArray() },
		{ TOOLBARCLASSNAME, GetToolBarStyleArray() },
		{ REBARCLASSNAME, GetRebarStyleArray() },
		{ WC_SCROLLBAR, GetScrollBarStyleArray(), L"SBS_HORZ" },
	};

	WCHAR name[64];
	if (::GetClassName(hWnd, name, _countof(name))) {
		// ATL's superclasses are named "ATL:<class>"
		auto bare = _wcsnicmp(name, L"ATL:", 4) == 0 ? name + 4 : name;
		for (auto& item : controls) {
			if (_wcsicmp(bare, item.className) == 0) {
				if (defaultStyle)
					*defaultStyle = item.def;
				return item.items;
			}
		}
	}
	if (defaultStyle)
		*defaultStyle = nullptr;
	return { nullptr, 0 };
}

namespace {
	CString Hex(ULONG_PTR value) {
		CString text;
		if (value)
			text.Format(L"0x%zX", value);
		else
			text = L"(None)";
		return text;
	}

	CString SystemColorName(int index) {
		static PCWSTR const names[] = {
			L"COLOR_SCROLLBAR", L"COLOR_BACKGROUND", L"COLOR_ACTIVECAPTION", L"COLOR_INACTIVECAPTION", L"COLOR_MENU",
			L"COLOR_WINDOW", L"COLOR_WINDOWFRAME", L"COLOR_MENUTEXT", L"COLOR_WINDOWTEXT", L"COLOR_CAPTIONTEXT",
			L"COLOR_ACTIVEBORDER", L"COLOR_INACTIVEBORDER", L"COLOR_APPWORKSPACE", L"COLOR_HIGHLIGHT", L"COLOR_HIGHLIGHTTEXT",
			L"COLOR_BTNFACE", L"COLOR_BTNSHADOW", L"COLOR_GRAYTEXT", L"COLOR_BTNTEXT", L"COLOR_INACTIVECAPTIONTEXT",
			L"COLOR_BTNHIGHLIGHT", L"COLOR_3DDKSHADOW", L"COLOR_3DLIGHT", L"COLOR_INFOTEXT", L"COLOR_INFOBK",
			L"(25)", L"COLOR_HOTLIGHT", L"COLOR_GRADIENTACTIVECAPTION", L"COLOR_GRADIENTINACTIVECAPTION",
			L"COLOR_MENUHILIGHT", L"COLOR_MENUBAR",
		};
		return index >= 0 && index < _countof(names) ? CString(names[index]) : CString();
	}

	CString ModuleName(HWND hWnd, HMODULE hModule) {
		DWORD pid;
		::GetWindowThreadProcessId(hWnd, &pid);
		auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (!hProcess)
			return L"";
		WCHAR path[MAX_PATH];
		auto len = ::GetModuleFileNameEx(hProcess, hModule, path, _countof(path));
		::CloseHandle(hProcess);
		return len ? CString(path) : CString();
	}
}

PropertyList WindowHelper::GetClassProperties(HWND hWnd) {
	PropertyList props;
	CString text;

	props.push_back({ L"Class Name", GetWindowClassName(hWnd) });
	text.Format(L"0x%04X", (DWORD)::GetClassLongPtr(hWnd, GCW_ATOM));
	props.push_back({ L"Atom", text });
	text.Format(L"0x%08X ", (DWORD)::GetClassLongPtr(hWnd, GCL_STYLE));
	props.push_back({ L"Style", text + ClassStyleToString(hWnd) });

	auto classProc = ::GetClassLongPtr(hWnd, GCLP_WNDPROC);
	auto windowProc = ::GetWindowLongPtr(hWnd, GWLP_WNDPROC);
	props.push_back({ L"Class Window Procedure", Hex(classProc) });
	// Windows doesn't give a window procedure of another process's window
	if (windowProc == 0)
		props.push_back({ L"Window Procedure", L"(Not available for another process's window)" });
	else
		props.push_back({ L"Window Procedure", Hex(windowProc) + (windowProc != classProc ? L" (subclassed)" : L"") });

	auto hModule = (HMODULE)::GetClassLongPtr(hWnd, GCLP_HMODULE);
	auto module = ModuleName(hWnd, hModule);
	props.push_back({ L"Module", Hex((ULONG_PTR)hModule) + (module.IsEmpty() ? CString() : L" " + module) });

	text.Format(L"%u", (DWORD)::GetClassLongPtr(hWnd, GCL_CBCLSEXTRA));
	props.push_back({ L"Class Extra Bytes", text });
	text.Format(L"%u", (DWORD)::GetClassLongPtr(hWnd, GCL_CBWNDEXTRA));
	props.push_back({ L"Window Extra Bytes", text });

	props.push_back({ L"Icon", Hex(::GetClassLongPtr(hWnd, GCLP_HICON)) });
	props.push_back({ L"Small Icon", Hex(::GetClassLongPtr(hWnd, GCLP_HICONSM)) });
	props.push_back({ L"Cursor", Hex(::GetClassLongPtr(hWnd, GCLP_HCURSOR)) });

	// a brush, or a system color index plus one
	auto brush = ::GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND);
	auto color = SystemColorName((int)brush - 1);
	props.push_back({ L"Background Brush", brush > 0 && brush <= COLOR_MENUBAR + 1 ? color + L" + 1" : Hex(brush) });
	return props;
}

PropertyList WindowHelper::GetWindowProps(HWND hWnd) {
	PropertyList props;
	::EnumPropsEx(hWnd, [](HWND, LPWSTR name, HANDLE data, ULONG_PTR param) -> BOOL {
		CString key, value;
		// an atom without a string, or a string
		if (IS_INTRESOURCE(name))
			key.Format(L"#%u", (UINT)(ULONG_PTR)name);
		else
			key = name;
		value.Format(L"0x%zX", (ULONG_PTR)data);
		reinterpret_cast<PropertyList*>(param)->push_back({ key, value });
		return TRUE;
		}, reinterpret_cast<LPARAM>(&props));
	std::sort(props.begin(), props.end(), [](auto& p1, auto& p2) { return p1.first.CompareNoCase(p2.first) < 0; });
	return props;
}

PropertyList WindowHelper::GetScrollProperties(HWND hWnd) {
	PropertyList props;
	CString text;

	auto add = [&](PCWSTR name, int bar, LONG objectId) {
		SCROLLBARINFO sbi{ sizeof(sbi) };
		if (::GetScrollBarInfo(hWnd, objectId, &sbi)) {
			auto state = sbi.rgstate[0];
			text = (state & STATE_SYSTEM_INVISIBLE) ? L"Hidden" : (state & STATE_SYSTEM_UNAVAILABLE) ? L"Disabled" : L"Visible";
		}
		else {
			text = L"(None)";
		}
		props.push_back({ name, text });

		SCROLLINFO si{ sizeof(si), SIF_ALL };
		if (!::GetScrollInfo(hWnd, bar, &si))
			return;
		CString prefix(name);
		text.Format(L"%d - %d", si.nMin, si.nMax);
		props.push_back({ prefix + L" Range", text });
		text.Format(L"%u", si.nPage);
		props.push_back({ prefix + L" Page", text });
		text.Format(L"%d", si.nPos);
		props.push_back({ prefix + L" Position", text });
		text.Format(L"%d", si.nTrackPos);
		props.push_back({ prefix + L" Track Position", text });
	};

	if (_wcsicmp(GetWindowClassName(hWnd), WC_SCROLLBAR) == 0)
		add(L"Scroll Bar Control", SB_CTL, OBJID_CLIENT);
	else {
		add(L"Horizontal", SB_HORZ, OBJID_HSCROLL);
		add(L"Vertical", SB_VERT, OBJID_VSCROLL);
	}
	return props;
}

int WindowHelper::ShowWindowProperties(HWND hWnd) {
	// not a window: a destroyed one, or a tree node that stands for a group (like the message-only windows)
	if (!::IsWindow(hWnd))
		return 0;

	CString text;
	text.Format(L"Window 0x%X Properties", PtrToUlong(hWnd));
	CWindowPropertiesSheet sheet((PCWSTR)text);
	sheet.m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	// the size it last had
	sheet.SetSize(AppSettings::Get().PropertiesSize());
	CWindowGeneralPage general(hWnd);
	CWindowStylesPage styles(hWnd);
	CWindowWindowsPage windows(hWnd);
	CPropertyListPage classPage(L"Class", IDI_WINDOW, hWnd, GetClassProperties, false);
	CPropertyListPage props(L"Properties", IDI_WINPROP, hWnd, GetWindowProps);
	CPropertyListPage scroll(L"Scroll", IDI_WINDOW, hWnd, GetScrollProperties);
	CPropertyListPage process(L"Process", IDI_PROCESS_INFO, hWnd, [](HWND h) {
		ProcessInfo pi;
		::GetWindowThreadProcessId(h, &pi.ProcessId);
		pi.ProcessName = ProcessHelper::GetProcessImageName(pi.ProcessId);
		return ProcessHelper::GetProcessProperties(pi);
		}, false);
	CPropertyListPage thread(L"Thread", IDI_PROCESSES, hWnd, [](HWND h) {
		return ProcessHelper::GetThreadProperties(::GetWindowThreadProcessId(h, nullptr));
		});
	sheet.AddPage(general);
	sheet.AddPage(styles);
	sheet.AddPage(windows);
	sheet.AddPage(classPage);
	sheet.AddPage(props);
	sheet.AddPage(scroll);
	sheet.AddPage(process);
	sheet.AddPage(thread);
	sheet.DoModal();
	AppSettings::Get().PropertiesSize(sheet.GetSize());

	return 0;
}

std::pair<StyleItem const*, int> WindowHelper::GetWindowStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(WS_POPUP),
		PAIR_STR(WS_CHILD),
		PAIR_STR(WS_MINIMIZE),
		PAIR_STR(WS_VISIBLE),
		PAIR_STR(WS_DISABLED),
		PAIR_STR(WS_CLIPSIBLINGS),
		PAIR_STR(WS_CLIPCHILDREN),
		PAIR_STR(WS_MAXIMIZE),
		PAIR_STR(WS_BORDER),
		PAIR_STR(WS_DLGFRAME),
		PAIR_STR(WS_VSCROLL),
		PAIR_STR(WS_HSCROLL),
		PAIR_STR(WS_SYSMENU),
		PAIR_STR(WS_THICKFRAME),
		PAIR_STR(WS_MINIMIZEBOX),
		PAIR_STR(WS_MAXIMIZEBOX),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetListViewStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(LVS_ALIGNLEFT),
		PAIR_STR(LVS_AUTOARRANGE),
		PAIR_STR(LVS_EDITLABELS),
		PAIR_STR(LVS_ICON),
		PAIR_STR(LVS_LIST),
		PAIR_STR(LVS_NOCOLUMNHEADER),
		PAIR_STR(LVS_NOLABELWRAP),
		PAIR_STR(LVS_SINGLESEL),
		PAIR_STR(LVS_REPORT),
		PAIR_STR(LVS_SMALLICON),
		PAIR_STR(LVS_SORTASCENDING),
		PAIR_STR(LVS_SORTDESCENDING),
		PAIR_STR(LVS_SHOWSELALWAYS),
		PAIR_STR(LVS_SHAREIMAGELISTS),
		PAIR_STR(LVS_OWNERDATA),
		PAIR_STR(LVS_OWNERDRAWFIXED),
		PAIR_STR(LVS_NOSCROLL),
		PAIR_STR(LVS_NOSORTHEADER),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetTreeViewStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(TVS_CHECKBOXES),
		PAIR_STR(TVS_DISABLEDRAGDROP),
		PAIR_STR(TVS_EDITLABELS),
		PAIR_STR(TVS_FULLROWSELECT),
		PAIR_STR(TVS_HASBUTTONS),
		PAIR_STR(TVS_HASLINES),
		PAIR_STR(TVS_LINESATROOT),
		PAIR_STR(TVS_NOHSCROLL),
		PAIR_STR(TVS_INFOTIP),
		PAIR_STR(TVS_NONEVENHEIGHT),
		PAIR_STR(TVS_NOTOOLTIPS),
		PAIR_STR(TVS_RTLREADING),
		PAIR_STR(TVS_SHOWSELALWAYS),
		PAIR_STR(TVS_SINGLEEXPAND),
		PAIR_STR(TVS_TRACKSELECT),
		PAIR_STR(TVS_NOSCROLL),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetTabCtrlStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(TCS_BOTTOM),
		PAIR_STR(TCS_BUTTONS),
		PAIR_STR(TCS_FIXEDWIDTH),
		PAIR_STR(TCS_FLATBUTTONS),
		PAIR_STR(TCS_FOCUSNEVER),
		PAIR_STR(TCS_FOCUSONBUTTONDOWN),
		PAIR_STR(TCS_FORCEICONLEFT),
		PAIR_STR(TCS_FORCELABELLEFT),
		PAIR_STR(TCS_HOTTRACK),
		PAIR_STR(TCS_MULTILINE),
		PAIR_STR(TCS_MULTISELECT),
		PAIR_STR(TCS_OWNERDRAWFIXED),
		PAIR_STR(TCS_RAGGEDRIGHT),
		PAIR_STR(TCS_RIGHT),
		PAIR_STR(TCS_RIGHTJUSTIFY),
		PAIR_STR(TCS_SCROLLOPPOSITE),
		PAIR_STR(TCS_SINGLELINE),
		PAIR_STR(TCS_TOOLTIPS),
		PAIR_STR(TCS_VERTICAL),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetWindowStyleExArray() {
	static const StyleItem styles[] = {
		PAIR_STR(WS_EX_DLGMODALFRAME),
		PAIR_STR(WS_EX_NOPARENTNOTIFY),
		PAIR_STR(WS_EX_TOPMOST),
		PAIR_STR(WS_EX_ACCEPTFILES),
		PAIR_STR(WS_EX_TRANSPARENT),
		PAIR_STR(WS_EX_TOOLWINDOW),
		PAIR_STR(WS_EX_WINDOWEDGE),
		PAIR_STR(WS_EX_CLIENTEDGE),
		PAIR_STR(WS_EX_CONTEXTHELP),
		PAIR_STR(WS_EX_RIGHT),
		PAIR_STR(WS_EX_RTLREADING),
		PAIR_STR(WS_EX_LEFTSCROLLBAR),
		PAIR_STR(WS_EX_CONTROLPARENT),
		PAIR_STR(WS_EX_STATICEDGE),
		PAIR_STR(WS_EX_APPWINDOW),
		PAIR_STR(WS_EX_LAYERED),
		PAIR_STR(WS_EX_NOINHERITLAYOUT),
		PAIR_STR(WS_EX_NOREDIRECTIONBITMAP),
		PAIR_STR(WS_EX_LAYOUTRTL),
		PAIR_STR(WS_EX_COMPOSITED),
		PAIR_STR(WS_EX_NOACTIVATE),
		{ 0x8000, L"WS_EX_FEEDBACK" },
		{ 0x80000000, L"WS_EX_ANSICREATOR" },
		{ 0x800000,	L"WS_EX_NOPADDEDBORDER" },
		{ 0x2,	L"WS_EX_DRAGOBJECT" },
	};
	
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetClassStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(CS_HREDRAW),
		PAIR_STR(CS_VREDRAW),
		PAIR_STR(CS_DBLCLKS),
		PAIR_STR(CS_OWNDC),
		PAIR_STR(CS_CLASSDC),
		PAIR_STR(CS_PARENTDC),
		PAIR_STR(CS_SAVEBITS),
		PAIR_STR(CS_GLOBALCLASS),
		PAIR_STR(CS_BYTEALIGNCLIENT),
		PAIR_STR(CS_BYTEALIGNWINDOW),
		PAIR_STR(CS_IME),
		PAIR_STR(CS_NOCLOSE),
		PAIR_STR(CS_DROPSHADOW),
		{ 0x8000, L"CS_SYSTEM" },
	};

	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetEditStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(ES_AUTOHSCROLL),
		PAIR_STR(ES_AUTOVSCROLL),
		PAIR_STR(ES_READONLY),
		PAIR_STR(ES_CENTER),
		PAIR_STR(ES_RIGHT),
		PAIR_STR(ES_NOHIDESEL),
		PAIR_STR(ES_LOWERCASE),
		PAIR_STR(ES_UPPERCASE),
		PAIR_STR(ES_WANTRETURN),
		PAIR_STR(ES_MULTILINE),
		PAIR_STR(ES_PASSWORD),
		PAIR_STR(ES_NUMBER),
		PAIR_STR(ES_OEMCONVERT),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetToolTipStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(TTS_ALWAYSTIP),
		PAIR_STR(TTS_BALLOON),
		PAIR_STR(TTS_CLOSE),
		PAIR_STR(TTS_NOANIMATE),
		PAIR_STR(TTS_NOFADE),
		PAIR_STR(TTS_NOPREFIX),
		PAIR_STR(TTS_USEVISUALSTYLE),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetScrollBarStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(SBS_VERT),
		PAIR_STR(SBS_SIZEBOX),
		PAIR_STR(SBS_SIZEGRIP),
		PAIR_STR(SBS_RIGHTALIGN),
		PAIR_STR(SBS_BOTTOMALIGN),
		PAIR_STR(SBS_TOPALIGN),
		PAIR_STR(SBS_LEFTALIGN),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetStatusBarStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(SBARS_SIZEGRIP),
		PAIR_STR(SBARS_TOOLTIPS),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetToolBarStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(TBSTYLE_ALTDRAG),
		PAIR_STR(TBSTYLE_FLAT),
		PAIR_STR(TBSTYLE_LIST),
		PAIR_STR(TBSTYLE_WRAPABLE),
		PAIR_STR(TBSTYLE_CUSTOMERASE),
		PAIR_STR(TBSTYLE_REGISTERDROP),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetRebarStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(RBS_AUTOSIZE),
		PAIR_STR(RBS_BANDBORDERS),
		PAIR_STR(RBS_DBLCLKTOGGLE),
		PAIR_STR(RBS_FIXEDORDER),
		PAIR_STR(RBS_VERTICALGRIPPER),
		PAIR_STR(RBS_REGISTERDROP),
		PAIR_STR(RBS_VARHEIGHT),
		PAIR_STR(RBS_TOOLTIPS),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetStaticStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR2(SS_RIGHT, SS_TYPEMASK),
		PAIR_STR2(SS_BLACKFRAME, SS_TYPEMASK),
		PAIR_STR2(SS_GRAYFRAME, SS_TYPEMASK),
		PAIR_STR2(SS_SIMPLE, SS_TYPEMASK),
		PAIR_STR2(SS_GRAYRECT, SS_TYPEMASK),
		PAIR_STR2(SS_BLACKRECT, SS_TYPEMASK),
		PAIR_STR2(SS_ICON, SS_TYPEMASK),
		PAIR_STR2(SS_BITMAP, SS_TYPEMASK),
		PAIR_STR2(SS_OWNERDRAW, SS_TYPEMASK),
		PAIR_STR2(SS_ENHMETAFILE, SS_TYPEMASK),
		PAIR_STR2(SS_ETCHEDFRAME, SS_TYPEMASK),
		PAIR_STR2(SS_ETCHEDHORZ, SS_TYPEMASK),
		PAIR_STR2(SS_ETCHEDVERT, SS_TYPEMASK),
		PAIR_STR2(SS_WHITEFRAME, SS_TYPEMASK),
		PAIR_STR2(SS_LEFTNOWORDWRAP, SS_TYPEMASK),
		PAIR_STR2(SS_CENTER, SS_TYPEMASK),

		PAIR_STR(SS_NOPREFIX),
		PAIR_STR(SS_NOTIFY),
		PAIR_STR(SS_CENTERIMAGE),
		PAIR_STR(SS_SUNKEN),
		PAIR_STR(SS_EDITCONTROL),
		PAIR_STR(SS_RIGHTJUST),
		PAIR_STR(SS_REALSIZEIMAGE),
		PAIR_STR2(SS_PATHELLIPSIS, SS_ELLIPSISMASK),
		PAIR_STR2(SS_WORDELLIPSIS, SS_ELLIPSISMASK),
		PAIR_STR2(SS_ENDELLIPSIS, SS_ELLIPSISMASK),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetHeaderStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(HDS_BUTTONS),
		PAIR_STR(HDS_CHECKBOXES),
		PAIR_STR(HDS_DRAGDROP),
		PAIR_STR(HDS_FILTERBAR),
		PAIR_STR(HDS_FLAT),
		PAIR_STR(HDS_FULLDRAG),
		PAIR_STR(HDS_HIDDEN),
		PAIR_STR(HDS_HOTTRACK),
		PAIR_STR(HDS_NOSIZING),
		PAIR_STR(HDS_OVERFLOW),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetButtonStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR2(BS_AUTO3STATE, BS_TYPEMASK),
		PAIR_STR2(BS_AUTOCHECKBOX, BS_TYPEMASK),
		PAIR_STR2(BS_AUTORADIOBUTTON, BS_TYPEMASK),
		PAIR_STR2(BS_GROUPBOX, BS_TYPEMASK),
		PAIR_STR2(BS_OWNERDRAW, BS_TYPEMASK),
		PAIR_STR2(BS_PUSHBOX, BS_TYPEMASK),
		PAIR_STR2(BS_CHECKBOX, BS_TYPEMASK),
		PAIR_STR2(BS_DEFPUSHBUTTON, BS_TYPEMASK),
		PAIR_STR2(BS_DEFSPLITBUTTON, BS_TYPEMASK),

		PAIR_STR(BS_CENTER),
		PAIR_STR(BS_RIGHT),
		PAIR_STR(BS_BITMAP),
		PAIR_STR(BS_BOTTOM),
		PAIR_STR(BS_COMMANDLINK),
		PAIR_STR(BS_FLAT),
		PAIR_STR(BS_NOTIFY),
		PAIR_STR(BS_MULTILINE),
		PAIR_STR(BS_TOP),
		PAIR_STR(BS_FLAT),
		PAIR_STR(BS_LEFTTEXT),
		PAIR_STR2(BS_USERBUTTON, BS_TYPEMASK),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetListBoxStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(LBS_COMBOBOX),
		PAIR_STR(LBS_NOSEL),
		PAIR_STR(LBS_SORT),
		PAIR_STR(LBS_NOTIFY),
		PAIR_STR(LBS_NOREDRAW),
		PAIR_STR(LBS_MULTICOLUMN),
		PAIR_STR(LBS_MULTIPLESEL),
		PAIR_STR(LBS_HASSTRINGS),
		PAIR_STR(LBS_USETABSTOPS),
		PAIR_STR(LBS_NOINTEGRALHEIGHT),
		PAIR_STR(LBS_OWNERDRAWFIXED),
		PAIR_STR(LBS_OWNERDRAWVARIABLE),
		PAIR_STR(LBS_WANTKEYBOARDINPUT),
		PAIR_STR(LBS_EXTENDEDSEL),
		PAIR_STR(LBS_DISABLENOSCROLL),
	};
	return std::make_pair(styles, (int)_countof(styles));
}

std::pair<StyleItem const*, int> WindowHelper::GetComboBoxStyleArray() {
	static const StyleItem styles[] = {
		PAIR_STR(CBS_AUTOHSCROLL),
		PAIR_STR(CBS_SIMPLE),
		PAIR_STR(CBS_SORT),
		PAIR_STR(CBS_DROPDOWN),
		PAIR_STR(CBS_DROPDOWNLIST),
		PAIR_STR(CBS_HASSTRINGS),
		PAIR_STR(CBS_HASSTRINGS),
		PAIR_STR(CBS_UPPERCASE),
		PAIR_STR(CBS_LOWERCASE),
		PAIR_STR(CBS_OWNERDRAWFIXED),
		PAIR_STR(CBS_OWNERDRAWVARIABLE),
	};
	return std::make_pair(styles, (int)_countof(styles));
}


CString WindowHelper::WindowMessageToString(DWORD msg) {
	switch (msg) {
		CASE_STR(WM_NULL);
		CASE_STR(WM_CREATE);
		CASE_STR(WM_DESTROY);
		CASE_STR(WM_MOVE);
		CASE_STR(WM_SIZE);
		CASE_STR(WM_ACTIVATE);
		CASE_STR(WM_SETFOCUS);
		CASE_STR(WM_KILLFOCUS);
		CASE_STR(WM_ENABLE);
		CASE_STR(WM_SETREDRAW);
		CASE_STR(WM_SETTEXT);
		CASE_STR(WM_GETTEXT);
		CASE_STR(WM_GETTEXTLENGTH);
		CASE_STR(WM_PAINT);
		CASE_STR(WM_CLOSE);
		CASE_STR(WM_QUERYENDSESSION);
		CASE_STR(WM_QUIT);
		CASE_STR(WM_QUERYOPEN);
		CASE_STR(WM_ERASEBKGND);
		CASE_STR(WM_SYSCOLORCHANGE);
		CASE_STR(WM_ENDSESSION);
		CASE_STR(WM_SHOWWINDOW);
		CASE_STR(WM_SETTINGCHANGE);
		CASE_STR(WM_DEVMODECHANGE);
		CASE_STR(WM_ACTIVATEAPP);
		CASE_STR(WM_FONTCHANGE);
		CASE_STR(WM_TIMECHANGE);
		CASE_STR(WM_CANCELMODE);
		CASE_STR(WM_SETCURSOR);
		CASE_STR(WM_MOUSEACTIVATE);
		CASE_STR(WM_CHILDACTIVATE);
		CASE_STR(WM_QUEUESYNC);
		CASE_STR(WM_GETMINMAXINFO);
		CASE_STR(WM_PAINTICON);
		CASE_STR(WM_ICONERASEBKGND);
		CASE_STR(WM_NEXTDLGCTL);
		CASE_STR(WM_SPOOLERSTATUS);
		CASE_STR(WM_DRAWITEM);
		CASE_STR(WM_MEASUREITEM);
		CASE_STR(WM_DELETEITEM);
		CASE_STR(WM_VKEYTOITEM);
		CASE_STR(WM_CHARTOITEM);
		CASE_STR(WM_SETFONT);
		CASE_STR(WM_GETFONT);
		CASE_STR(WM_SETHOTKEY);
		CASE_STR(WM_GETHOTKEY);
		CASE_STR(WM_QUERYDRAGICON);
		CASE_STR(WM_COMPAREITEM);
		CASE_STR(WM_GETOBJECT);
		CASE_STR(WM_COMPACTING);
		CASE_STR(WM_COMMNOTIFY);
		CASE_STR(WM_WINDOWPOSCHANGING);
		CASE_STR(WM_WINDOWPOSCHANGED);
		CASE_STR(WM_POWER);
		CASE_STR(WM_COPYDATA);
		CASE_STR(WM_CANCELJOURNAL);
		CASE_STR(WM_NOTIFY);
		CASE_STR(WM_INPUTLANGCHANGEREQUEST);
		CASE_STR(WM_INPUTLANGCHANGE);
		CASE_STR(WM_TCARD);
		CASE_STR(WM_HELP);
		CASE_STR(WM_USERCHANGED);
		CASE_STR(WM_NOTIFYFORMAT);
		CASE_STR(WM_CONTEXTMENU);
		CASE_STR(WM_STYLECHANGING);
		CASE_STR(WM_STYLECHANGED);
		CASE_STR(WM_DISPLAYCHANGE);
		CASE_STR(WM_GETICON);
		CASE_STR(WM_SETICON);
		CASE_STR(WM_NCCREATE);
		CASE_STR(WM_NCDESTROY);
		CASE_STR(WM_NCCALCSIZE);
		CASE_STR(WM_NCHITTEST);
		CASE_STR(WM_NCPAINT);
		CASE_STR(WM_NCACTIVATE);
		CASE_STR(WM_GETDLGCODE);
		CASE_STR(WM_SYNCPAINT);
		CASE_STR(WM_NCMOUSEMOVE);
		CASE_STR(WM_NCLBUTTONDOWN);
		CASE_STR(WM_NCLBUTTONUP);
		CASE_STR(WM_NCLBUTTONDBLCLK);
		CASE_STR(WM_NCRBUTTONDOWN);
		CASE_STR(WM_NCRBUTTONUP);
		CASE_STR(WM_NCRBUTTONDBLCLK);
		CASE_STR(WM_NCMBUTTONDOWN);
		CASE_STR(WM_NCMBUTTONUP);
		CASE_STR(WM_NCMBUTTONDBLCLK);
		CASE_STR(WM_NCXBUTTONDOWN);
		CASE_STR(WM_NCXBUTTONUP);
		CASE_STR(WM_NCXBUTTONDBLCLK);
		CASE_STR(WM_INPUT);
		CASE_STR(WM_KEYDOWN);
		CASE_STR(WM_KEYUP);
		CASE_STR(WM_CHAR);
		CASE_STR(WM_DEADCHAR);
		CASE_STR(WM_SYSKEYDOWN);
		CASE_STR(WM_SYSKEYUP);
		CASE_STR(WM_SYSCHAR);
		CASE_STR(WM_SYSDEADCHAR);
		CASE_STR(WM_KEYLAST);
		CASE_STR(WM_IME_STARTCOMPOSITION);
		CASE_STR(WM_IME_ENDCOMPOSITION);
		CASE_STR(WM_IME_COMPOSITION);
		CASE_STR(WM_INITDIALOG);
		CASE_STR(WM_COMMAND);
		CASE_STR(WM_SYSCOMMAND);
		CASE_STR(WM_TIMER);
		CASE_STR(WM_HSCROLL);
		CASE_STR(WM_VSCROLL);
		CASE_STR(WM_INITMENU);
		CASE_STR(WM_INITMENUPOPUP);
		CASE_STR(WM_GESTURE);
		CASE_STR(WM_GESTURENOTIFY);
		CASE_STR(WM_MENUSELECT);
		CASE_STR(WM_MENUCHAR);
		CASE_STR(WM_ENTERIDLE);
		CASE_STR(WM_UNINITMENUPOPUP);
		CASE_STR(WM_CHANGEUISTATE);
		CASE_STR(WM_UPDATEUISTATE);
		CASE_STR(WM_QUERYUISTATE);
		CASE_STR(WM_CTLCOLORMSGBOX);
		CASE_STR(WM_CTLCOLOREDIT);
		CASE_STR(WM_CTLCOLORLISTBOX);
		CASE_STR(WM_CTLCOLORBTN);
		CASE_STR(WM_CTLCOLORDLG);
		CASE_STR(WM_CTLCOLORSCROLLBAR);
		CASE_STR(WM_CTLCOLORSTATIC);
		CASE_STR(WM_MOUSEMOVE);
		CASE_STR(WM_LBUTTONDOWN);
		CASE_STR(WM_LBUTTONUP);
		CASE_STR(WM_LBUTTONDBLCLK);
		CASE_STR(WM_RBUTTONDOWN);
		CASE_STR(WM_RBUTTONUP);
		CASE_STR(WM_RBUTTONDBLCLK);
		CASE_STR(WM_MBUTTONDOWN);
		CASE_STR(WM_MBUTTONUP);
		CASE_STR(WM_MBUTTONDBLCLK);
		CASE_STR(WM_MOUSEWHEEL);
		CASE_STR(WM_XBUTTONDOWN);
		CASE_STR(WM_XBUTTONUP);
		CASE_STR(WM_XBUTTONDBLCLK);
		CASE_STR(WM_MOUSEHWHEEL);
		CASE_STR(WM_PARENTNOTIFY);
		CASE_STR(WM_ENTERMENULOOP);
		CASE_STR(WM_EXITMENULOOP);
		CASE_STR(WM_NEXTMENU);
		CASE_STR(WM_SIZING);
		CASE_STR(WM_CAPTURECHANGED);
		CASE_STR(WM_MOVING);
		CASE_STR(WM_POWERBROADCAST);
		CASE_STR(WM_DEVICECHANGE);
		CASE_STR(WM_POINTERDEVICECHANGE);
		CASE_STR(WM_POINTERDEVICEINRANGE);
		CASE_STR(WM_POINTERDEVICEOUTOFRANGE);
		CASE_STR(WM_POINTERUPDATE);
		CASE_STR(WM_POINTERDOWN);
		CASE_STR(WM_POINTERUP);
		CASE_STR(WM_POINTERENTER);
		CASE_STR(WM_POINTERLEAVE);
		CASE_STR(WM_POINTERACTIVATE);
		CASE_STR(WM_POINTERCAPTURECHANGED);
		CASE_STR(WM_IME_SETCONTEXT);
		CASE_STR(WM_IME_NOTIFY);
		CASE_STR(WM_IME_CONTROL);
		CASE_STR(WM_IME_COMPOSITIONFULL);
		CASE_STR(WM_IME_SELECT);
		CASE_STR(WM_IME_CHAR);
		CASE_STR(WM_IME_REQUEST);
		CASE_STR(WM_IME_KEYDOWN);
		CASE_STR(WM_IME_KEYUP);
		CASE_STR(WM_MDICREATE);
		CASE_STR(WM_MDIDESTROY);
		CASE_STR(WM_MDIACTIVATE);
		CASE_STR(WM_MDIRESTORE);
		CASE_STR(WM_MDINEXT);
		CASE_STR(WM_MDIMAXIMIZE);
		CASE_STR(WM_MDITILE);
		CASE_STR(WM_MDICASCADE);
		CASE_STR(WM_MDIICONARRANGE);
		CASE_STR(WM_MDIGETACTIVE);
		CASE_STR(WM_MDISETMENU);
		CASE_STR(WM_ENTERSIZEMOVE);
		CASE_STR(WM_EXITSIZEMOVE);
		CASE_STR(WM_DROPFILES);
		CASE_STR(WM_MDIREFRESHMENU);
		CASE_STR(WM_MOUSEHOVER);
		CASE_STR(WM_NCMOUSELEAVE);
		CASE_STR(WM_MOUSELEAVE);
		CASE_STR(WM_WTSSESSION_CHANGE);
		CASE_STR(WM_DPICHANGED);
		CASE_STR(WM_DPICHANGED_BEFOREPARENT);
		CASE_STR(WM_DPICHANGED_AFTERPARENT);
		CASE_STR(WM_CUT);
		CASE_STR(WM_COPY);
		CASE_STR(WM_PASTE);
		CASE_STR(WM_CLEAR);
		CASE_STR(WM_UNDO);
		CASE_STR(WM_RENDERFORMAT);
		CASE_STR(WM_RENDERALLFORMATS);
		CASE_STR(WM_DESTROYCLIPBOARD);
		CASE_STR(WM_DRAWCLIPBOARD);
		CASE_STR(WM_PAINTCLIPBOARD);
		CASE_STR(WM_VSCROLLCLIPBOARD);
		CASE_STR(WM_SIZECLIPBOARD);
		CASE_STR(WM_ASKCBFORMATNAME);
		CASE_STR(WM_CHANGECBCHAIN);
		CASE_STR(WM_HSCROLLCLIPBOARD);
		CASE_STR(WM_QUERYNEWPALETTE);
		CASE_STR(WM_PALETTEISCHANGING);
		CASE_STR(WM_PALETTECHANGED);
		CASE_STR(WM_HOTKEY);
		CASE_STR(WM_PRINT);
		CASE_STR(WM_PRINTCLIENT);
		CASE_STR(WM_APPCOMMAND);
		CASE_STR(WM_THEMECHANGED);
		CASE_STR(WM_DWMCOMPOSITIONCHANGED);
		CASE_STR(WM_DWMNCRENDERINGCHANGED);
		CASE_STR(WM_DWMCOLORIZATIONCOLORCHANGED);
		CASE_STR(WM_DWMWINDOWMAXIMIZEDCHANGE);
		CASE_STR(WM_HANDHELDFIRST);
		CASE_STR(WM_HANDHELDLAST);
		CASE_STR(WM_AFXFIRST);
		CASE_STR(WM_AFXLAST);
		CASE_STR(WM_PENWINFIRST);
		CASE_STR(WM_PENWINLAST);
		CASE_STR(WM_DWMSENDICONICTHUMBNAIL);
		CASE_STR(WM_DWMSENDICONICLIVEPREVIEWBITMAP);
		CASE_STR(WM_USER);
		CASE_STR(WM_APP);
		case 0x0118: return L"WM_SYSTIMER";
		// undocumented: the themed non-client area's caption and frame
		case 0x00AE: return L"WM_NCUAHDRAWCAPTION";
		case 0x00AF: return L"WM_NCUAHDRAWFRAME";
		default:
			if (msg > WM_USER && msg < WM_APP) {
				CString text;
				text.Format(L"WM_USER + %d", msg - WM_USER);
				return text;
			}
			if (msg > WM_APP) {
				CString text;
				text.Format(L"WM_APP + %d", msg - WM_APP);
				return text;
			}
			break;
	}
	return L"";
}

ULONG_PTR WindowHelper::GetID(HWND hWnd) {
	return ::GetWindowLongPtr(hWnd, GWLP_ID);
}

ULONG_PTR WindowHelper::GetUserData(HWND hWnd) {
	return ::GetWindowLongPtr(hWnd, GWLP_USERDATA);
}

void WindowEvents::Subscribe(IWindowEventSink* sink) {
	if (s_Sinks.empty()) {
		// narrow ranges: EVENT_OBJECT_LOCATIONCHANGE, between them, comes all the time
		static const std::pair<DWORD, DWORD> ranges[] = {
			{ EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE },
			{ EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE },
			{ EVENT_OBJECT_PARENTCHANGE, EVENT_OBJECT_PARENTCHANGE },
		};
		for (auto [first, last] : ranges)
			if (auto hHook = ::SetWinEventHook(first, last, nullptr, OnEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS))
				s_Hooks.push_back(hHook);
	}
	s_Sinks.push_back(sink);
}

void WindowEvents::Unsubscribe(IWindowEventSink* sink) {
	std::erase(s_Sinks, sink);
	if (s_Sinks.empty()) {
		for (auto hHook : s_Hooks)
			::UnhookWinEvent(hHook);
		s_Hooks.clear();
	}
}

void CALLBACK WindowEvents::OnEvent(HWINEVENTHOOK, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD, DWORD) {
	// the window itself, not one of its parts
	if (!hWnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
		return;

	// a copy: a sink may unsubscribe while handling the event
	auto sinks = s_Sinks;
	for (auto sink : sinks)
		sink->OnWindowEvent(event, hWnd);
}
