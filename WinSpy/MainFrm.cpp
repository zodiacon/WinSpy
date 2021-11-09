// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "aboutdlg.h"
#include "WindowsView.h"
#include "MainFrm.h"
#include "FindWindowDlg.h"
#include "IconHelper.h"
#include "ImageIconCache.h"
#include "ProcessesView.h"

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
	// create command bar window
	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	// attach menu
	m_CmdBar.AttachMenu(GetMenu());
	// remove old menu
	SetMenu(nullptr);

	m_CmdBar.SetAlphaImages(true);
	InitCommandBar();

	CToolBarCtrl tb;
	auto hWndToolBar = tb.Create(m_hWnd, nullptr, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
	tb.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);
	InitToolBar(tb);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(hWndToolBar, nullptr, TRUE);

	CReBarCtrl rb(m_hWndToolBar);
	rb.LockBands(true);

	CreateSimpleStatusBar();

	m_view.m_bTabCloseButton = false;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);

	UIAddMenu(m_CmdBar.GetMenu());
	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	ImageIconCache::Get().SetImageList(WindowHelper::GetImageList());

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	CMenuHandle menuMain = m_CmdBar.GetMenu();
	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 4, 4);
	images.AddIcon(AtlLoadIconImage(IDI_WINDOWS, 0, 16, 16));
	images.AddIcon(AtlLoadIconImage(IDI_PROCESSES, 0, 16, 16));
	m_view.SetImageList(images);

	m_view.SetWindowMenu(menuMain.GetSubMenu(WINDOW_MENU_POSITION));

	PostMessage(WM_COMMAND, ID_VIEW_ALLWINDOWS);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
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
		m_view.AddPage(pView->m_hWnd, _T("Windows Tree"), 0, pView);
	}
	pView->OnActivate(true);

	return 0;
}

LRESULT CMainFrame::OnViewAllProcesses(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pView = new CProcessesView(this);
	{
		CWaitCursor wait;
		pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
		m_view.AddPage(pView->m_hWnd, _T("Processes"), 1, pView);
	}
	pView->OnActivate(true);

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

CUpdateUIBase& CMainFrame::GetUIUpdate() {
	return *this;
}

UINT CMainFrame::ShowContextMenu(HMENU hMenu, const POINT& pt, DWORD flags) {
	return (UINT)m_CmdBar.TrackPopupMenu(hMenu, flags, pt.x, pt.y);
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

void CMainFrame::InitCommandBar() {
	struct {
		UINT id, icon;
		HICON hIcon = nullptr;
	} cmds[] = {
		{ ID_FILE_RUNASADMINISTRATOR, 0, IconHelper::GetShieldIcon() },
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
	};
	for (auto& cmd : cmds) {
		m_CmdBar.AddIcon(cmd.icon ? AtlLoadIconImage(cmd.icon, 0, 16, 16) : cmd.hIcon, cmd.id);
	}
}

LRESULT CMainFrame::OnTabActivated(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/) {
	if (m_ActivePage >= 0 && m_ActivePage < m_view.GetPageCount())
		::SendMessage(m_view.GetPageHWND(m_ActivePage), TBVN_PAGEACTIVATED, 0, 0);

	m_ActivePage = m_view.GetActivePage();
	if (m_ActivePage < 0)
		return 0;

	return ::SendMessage(m_view.GetPageHWND(m_ActivePage), TBVN_PAGEACTIVATED, 1, 0);
}
