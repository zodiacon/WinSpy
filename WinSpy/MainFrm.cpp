// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "aboutdlg.h"
#include "WindowsView.h"
#include "MainFrm.h"
#include "FindWindowDlg.h"
#include <IconHelper.h>
#include "ImageIconCache.h"
#include "ProcessesView.h"
#include "SecurityHelper.h"
#include "MessagesView.h"
#include "AutomationTreeView.h"
#include "AppSettings.h"
#include "AllWindowsView.h"
#include "ProcessHelper.h"
#include <WTLHelper.h>
#include <ClipboardHelper.h>
#include <ListViewhelper.h>

const int WINDOW_MENU_POSITION = 5;

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (SecurityHelper::IsRunningElevated()) {
		CMenuHandle menu = GetMenu();
		menu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
		menu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
	}
	InitMenu(GetMenu());
	UIAddMenu(GetMenu());
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());

	if (AppSettings::Get().AlwaysOnTop()) {
		SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		UISetCheck(ID_OPTIONS_ALWAYSONTOP, TRUE);
	}
	if (auto lf = AppSettings::Get().ViewFont(); lf.lfFaceName[0])
		m_Font.CreateFontIndirect(&lf);

	CToolBarCtrl tb;
	auto hWndToolBar = tb.Create(m_hWnd, nullptr, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
	tb.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);
	InitToolBar(tb);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndToolBar, nullptr, TRUE);

	CReBarCtrl rb(m_hWndToolBar);
	rb.LockBands(true);

	CreateSimpleStatusBar(L"Ready - Ctrl+Alt+F12 anywhere locates the window under the mouse cursor");
	// (another program may have the keys already; WinSpy works without them)
	::RegisterHotKey(m_hWnd, HotKeyLocate, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F12);

	//m_view.m_bTabCloseButton = false;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);

	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	ImageIconCache::Get().SetImageList(WindowHelper::GetImageList());

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 4, 4);
	UINT icons[] = { IDI_WINDOWS, IDI_PROCESSES, IDI_MESSAGES, IDI_AUTOMATION };
	for (auto icon : icons) {
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	}
	m_view.SetImageList(images);
	m_view.SetWindowMenu(((CMenuHandle)GetMenu()).GetSubMenu(WINDOW_MENU_POSITION));

	PostMessage(WM_COMMAND, ID_VIEW_ALLWINDOWS);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	::UnregisterHotKey(m_hWnd, HotKeyLocate);
	AppSettings::Get().Save();

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnMenuSelect(UINT, WPARAM, LPARAM, BOOL&) {
	return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnViewAllWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pView = new CWindowsView(this);
	{
		CWaitCursor wait;
		pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
		AddView(pView->m_hWnd, L"Windows Tree", 0, pView);
	}

	return 0;
}

LRESULT CMainFrame::OnViewAllProcesses(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pView = new CProcessesView(this);
	{
		CWaitCursor wait;
		pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
		AddView(pView->m_hWnd, L"Processes", 1, pView);
	}

	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	static BOOL bVisible = TRUE;	// initially visible
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nActivePage = m_view.GetActivePage();
	if (nActivePage != -1)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_view.RemoveAllPages();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}

LRESULT CMainFrame::OnCommandToActiveView(WORD code, WORD id, HWND h, BOOL&) {
	auto page = m_view.GetActivePage();
	if (page < 0)
		return 0;

	return ::SendMessage(m_view.GetPageHWND(page), WM_COMMAND, MAKELONG(id, code), reinterpret_cast<LPARAM>(h));
}

LRESULT CMainFrame::OnFindWindow(WORD, WORD, HWND, BOOL&) {
	CFindWindowDlg dlg(this);
	if (IDOK == dlg.DoModal()) {
		WindowHelper::ShowWindowProperties(dlg.GetSelectedHwnd());
	}

	return 0;
}

LRESULT CMainFrame::OnRunAsAdmin(WORD, WORD, HWND, BOOL&) {
	if (SecurityHelper::RunElevated()) {
		SendMessage(WM_CLOSE);
	}
	return 0;
}

LRESULT CMainFrame::OnTabCloseButton(int, LPNMHDR, BOOL&) {
	return 0;
}

LRESULT CMainFrame::OnViewAutomationTree(WORD, WORD, HWND, BOOL&) {
	auto pView = new CAutomationTreeView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	AddView(pView->m_hWnd, L"UI Automation", 3, pView);
	return 0;
}

CUpdateUIBase& CMainFrame::GetUIUpdate() {
	return *this;
}

UINT CMainFrame::ShowPopupMenu(HMENU hMenu, const POINT& pt, DWORD flags) {
	InitMenu(hMenu);
	return (UINT)::TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, m_hWnd, nullptr);
}

void CMainFrame::AddView(HWND hView, PCWSTR title, int image, void* data) {
	m_view.AddPage(hView, title, image, data);
	if (m_Font)
		ApplyFont(hView);
}

void CMainFrame::ApplyFont(HWND hRoot) {
	// the trees and the lists only (not the toolbars, tabs or status bar)
	::EnumChildWindows(hRoot, [](auto hWnd, auto font) {
		WCHAR className[32];
		if (::GetClassName(hWnd, className, _countof(className))
			&& (_wcsicmp(className, WC_TREEVIEW) == 0 || _wcsicmp(className, WC_LISTVIEW) == 0))
			::SendMessage(hWnd, WM_SETFONT, (WPARAM)font, TRUE);
		return TRUE;
		}, (LPARAM)(HFONT)m_Font);
}

LRESULT CMainFrame::OnViewWindowsList(WORD, WORD, HWND, BOOL&) {
	auto pView = new CAllWindowsView(this);
	CWaitCursor wait;
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	AddView(pView->m_hWnd, L"Windows List", 0, pView);
	return 0;
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD, WORD, HWND, BOOL&) {
	bool onTop = (GetExStyle() & WS_EX_TOPMOST) == 0;
	SetWindowPos(onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, onTop);
	AppSettings::Get().AlwaysOnTop(onTop);
	return 0;
}

LRESULT CMainFrame::OnFonts(WORD, WORD, HWND, BOOL&) {
	LOGFONT lf = AppSettings::Get().ViewFont();
	if (lf.lfFaceName[0] == 0)
		::GetObject(AtlGetDefaultGuiFont(), sizeof(lf), &lf);

	CFontDialog dlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT);
	if (dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	AppSettings::Get().ViewFont(dlg.m_lf);
	m_Font.DeleteObject();
	m_Font.CreateFontIndirect(&dlg.m_lf);
	ApplyFont(m_view);
	return 0;
}

LRESULT CMainFrame::OnCopy(WORD, WORD, HWND, BOOL&) {
	// the selection of the tree or list that has the focus
	auto hFocus = ::GetFocus();
	WCHAR className[32];
	if (!hFocus || !::GetClassName(hFocus, className, _countof(className)))
		return 0;

	CString text;
	if (_wcsicmp(className, WC_TREEVIEW) == 0) {
		CTreeViewCtrl tv(hFocus);
		if (auto hItem = tv.GetSelectedItem())
			tv.GetItemText(hItem, text);
	}
	else if (_wcsicmp(className, WC_LISTVIEW) == 0) {
		text = ListViewHelper::GetSelectedRowsAsString(CListViewCtrl(hFocus));
	}
	else if (_wcsicmp(className, WC_EDIT) == 0) {
		::SendMessage(hFocus, WM_COPY, 0, 0);
		return 0;
	}

	if (text.IsEmpty())
		::MessageBeep(MB_ICONWARNING);
	else
		ClipboardHelper::CopyText(m_hWnd, text);
	return 0;
}

LRESULT CMainFrame::OnHotKey(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id != HotKeyLocate)
		return 0;

	POINT pt;
	::GetCursorPos(&pt);
	auto hWnd = ::WindowFromPoint(pt);
	DWORD pid = 0;
	if (hWnd)
		::GetWindowThreadProcessId(hWnd, &pid);
	if (!hWnd || pid == ::GetCurrentProcessId()) {
		::MessageBeep(MB_ICONWARNING);
		return 0;
	}

	// a hot key lets WinSpy come to the foreground
	if (IsIconic())
		ShowWindow(SW_RESTORE);
	::SetForegroundWindow(m_hWnd);
	WindowHelper::HighlightWindow(hWnd);
	LocateInWindowsTree(hWnd);
	return 0;
}

bool CMainFrame::CaptureMessages(HWND hWnd, DWORD threadId, DWORD processId) {
	// the process the messages come from
	DWORD pid = processId;
	if (hWnd)
		::GetWindowThreadProcessId(hWnd, &pid);
	else if (threadId) {
		auto hThread = ::OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId);
		if (hThread) {
			pid = ::GetProcessIdOfThread(hThread);
			::CloseHandle(hThread);
		}
	}

	if (pid == ::GetCurrentProcessId()) {
		// its own messages would make more messages, without end
		AtlMessageBox(m_hWnd, L"WinSpy can't capture its own messages.", IDS_TITLE, MB_ICONWARNING);
		return false;
	}

	// the hook DLL can only be loaded into processes of its own bitness
	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (hProcess) {
		BOOL targetWow64 = FALSE, wow64 = FALSE;
		::IsWow64Process(hProcess, &targetWow64);
		::IsWow64Process(::GetCurrentProcess(), &wow64);
		::CloseHandle(hProcess);
		if (targetWow64 != wow64) {
			AtlMessageBox(m_hWnd, targetWow64 ? L"This is a 32-bit process; this WinSpy can capture messages of 64-bit processes only."
				: L"This is a 64-bit process; this WinSpy can capture messages of 32-bit processes only.", IDS_TITLE, MB_ICONWARNING);
			return false;
		}
	}

	CString title;
	if (hWnd)
		title.Format(L"Messages: 0x%zX", (DWORD_PTR)hWnd);
	else if (threadId)
		title.Format(L"Messages: Thread %u", threadId);
	else
		title.Format(L"Messages: %s (%u)", (PCWSTR)ProcessHelper::GetProcessImageName(pid), pid);

	auto pView = new CMessagesView(this);
	pView->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	AddView(pView->m_hWnd, title, 2, pView);

	bool ok = hWnd ? pView->CaptureWindow(hWnd) : threadId ? pView->CaptureThread(threadId) : pView->CaptureProcess(processId);
	if (!ok) {
		AtlMessageBox(m_hWnd, L"Failed to install the message hooks (are too many captures open?)", IDS_TITLE, MB_ICONERROR);
		CloseTab(pView);
	}
	return ok;
}

void CMainFrame::LocateInWindowsTree(HWND hWnd) {
	// a windows tree tab: the active one if it is one, otherwise the first; a new one if there is none
	int count = m_view.GetPageCount(), active = m_view.GetActivePage();
	for (int n = 0; n < count; n++) {
		int page = active < 0 ? n : (active + n) % count;
		auto result = (LocateResult)::SendMessage(m_view.GetPageHWND(page), WM_LOCATE_WINDOW, 0, (LPARAM)hWnd);
		if (result == LocateResult::NotHandled)
			continue;

		if (result == LocateResult::NotInTree)
			AtlMessageBox(m_hWnd, L"The window is not in the tree (it may be hidden or have no title - see the View menu).", IDS_TITLE, MB_ICONINFORMATION);
		else if (page != active)
			m_view.SetActivePage(page);
		return;
	}

	SendMessage(WM_COMMAND, ID_VIEW_ALLWINDOWS);
	if (auto page = m_view.GetActivePage(); page >= 0 && (LocateResult)::SendMessage(m_view.GetPageHWND(page), WM_LOCATE_WINDOW, 0, (LPARAM)hWnd) == LocateResult::NotInTree)
		AtlMessageBox(m_hWnd, L"The window is not in the tree (it may be hidden or have no title - see the View menu).", IDS_TITLE, MB_ICONINFORMATION);
}

void CMainFrame::CloseTab(CWindow* win) {
	m_view.RemovePage(m_view.GetActivePage());
}

void CMainFrame::InitToolBar(CToolBarCtrl& tb) {
	CImageList tbImages;
	tbImages.Create(24, 24, ILC_COLOR32, 8, 4);
	tb.SetImageList(tbImages);

	struct {
		UINT id;
		int image;
		int style = BTNS_BUTTON;
		PCWSTR text = nullptr;
	} buttons[] = {
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ 0 },
		{ ID_VIEW_ALLWINDOWS, IDI_WINDOWS },
		{ ID_VIEW_ALLPROCESSES, IDI_PROCESSES },
		{ ID_WINDOW_FIND, IDI_WINDOWSEARCH },
		{ 0 },
		{ ID_VIEW_HIDDENWINDOWS, IDI_WINDOW_HIDDEN },
		{ ID_VIEW_EMPTYTITLEWINDOWS, IDI_WINDOW_NOTEXT },
		{ ID_WINDOW_PROPERTIES, IDI_WINPROP },
	};
	for (auto& b : buttons) {
		if (b.id == 0)
			tb.AddSeparator(0);
		else {
			int image = tbImages.AddIcon(AtlLoadIconImage(b.image, 0, 24, 24));
			tb.AddButton(b.id, b.style, TBSTATE_ENABLED, image, b.text, 0);
		}
	}
}

void CMainFrame::InitMenu(HMENU menu) {
	static HICON hShieldIcon = IconHelper::GetShieldIcon();
	MenuItemData cmds[] = {
		{ ID_FILE_RUNASADMINISTRATOR, 0, hShieldIcon },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_VIEW_ALLWINDOWS, IDI_WINDOWS },
		{ ID_VIEW_HIDDENWINDOWS, IDI_WINDOW_HIDDEN },
		{ ID_VIEW_EMPTYTITLEWINDOWS, IDI_WINDOW_NOTEXT },
		{ ID_WINDOW_CLOSE, IDI_WINDOW_CLOSE },
		{ ID_STATE_CLOSE, IDI_WINDOW_CLOSE },
		{ ID_WINDOW_MINIMIZE, IDI_WINDOW_MINIMIZE },
		{ ID_WINDOW_MAXIMIZE, IDI_WINDOW_MAXIMIZE },
		{ ID_VIEW_ALLPROCESSES, IDI_PROCESSES },
		{ ID_WINDOW_PROPERTIES, IDI_WINPROP },
		{ ID_PROCESS_PROPERTIES, IDI_PROCESS_INFO },
		{ ID_WINDOW_FIND, IDI_WINDOWSEARCH },
		{ ID_WINDOW_RESTORE, IDI_RESTORE },
		{ ID_TREE_SENDTOBACK, IDI_WINDOW_SENDTOBACK },
		{ ID_WINDOW_BRINGTOFRONT, IDI_SENDTOFRONT },
	};
	WTLHelper::InitMenu(menu, cmds, _countof(cmds));
}

LRESULT CMainFrame::OnToggleDarkMode(WORD, WORD, HWND, BOOL&) {
	WTLHelper::SwitchToMode(WTLHelper::IsDarkMode() ? DarkModeKind::Classic : DarkModeKind::Dark, m_hWnd);
	AppSettings::Get().DarkMode(WTLHelper::IsDarkMode() ? 1 : 0);
	InitMenu(GetMenu());
	DrawMenuBar();
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());
	return 0;
}

LRESULT CMainFrame::OnTabActivated(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/) {
	//
	// the previous page is kept by its window, as page indices shift when pages are removed
	// (a removed page's window is destroyed, so it's not told anything)
	//
	if (::IsWindow(m_hActivePage))
		::SendMessage(m_hActivePage, TBVN_PAGEACTIVATED, 0, 0);

	int page = m_view.GetActivePage();
	m_hActivePage = page < 0 ? nullptr : m_view.GetPageHWND(page);
	if (!m_hActivePage)
		return 0;

	return ::SendMessage(m_hActivePage, TBVN_PAGEACTIVATED, 1, 0);
}
