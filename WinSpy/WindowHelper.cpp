#include "pch.h"
#include "resource.h"
#include "WindowHelper.h"
#include "ProcessHelper.h"
#include "FormatHelper.h"
#include "WindowGeneralPage.h"
#include "WindowWindowsPage.h"

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

	CRgn rgn1;
	rc.InflateRect(2, 2);
	rgn1.CreateRectRgnIndirect(&rc);
	rc.DeflateRect(5, 5);
	CRgn rgn2;
	rgn2.CreateRectRgnIndirect(&rc);
	CRgn rgn;
	rgn.CreateRectRgn(0, 0, 1, 1);
	rgn.CombineRgn(rgn1, rgn2, RGN_DIFF);
	if (!highlight) {
		::RedrawWindow(hWnd, nullptr, rgn, RDW_INTERNALPAINT | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
		return;
	}

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
	sheet.m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	CWindowGeneralPage general(hWnd);
	CWindowWindowsPage windows(hWnd);
	sheet.AddPage(general);
	sheet.AddPage(windows);
	sheet.DoModal();

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

