// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "TreeViewManager.h"
#include "ViewBase.h"
#include "WindowsListView.h"
#include "WindowCommands.h"
#include <CustomSplitterWindow.h>

class CWindowsView : 
	public CViewBase<CWindowsView>,
	public CTreeViewManager<CWindowsView>,
	public IWindowEventSink {
public:
	CWindowsView(IMainFrame* frame) : CViewBase(frame), m_WindowsView(frame) {}

	BOOL PreTranslateMessage(MSG* pMsg);

	void OnActivate(bool activate);
	LRESULT OnTreeNodeDoubleClick(HTREEITEM hItem, CPoint const& pt);

protected:
	enum { IDC_TREE = 123 };

	BEGIN_MSG_MAP(CWindowsView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_HANDLER(IDC_SEARCH, EN_CHANGE, OnSearchChanged)
		COMMAND_ID_HANDLER(ID_EDIT_FIND, OnFind)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		NOTIFY_CODE_HANDLER(TVN_DELETEITEM, OnNodeDeleted)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnNodeSelected)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		COMMAND_ID_HANDLER(ID_VIEW_HIDDENWINDOWS, OnToggleHiddenWindows)
		COMMAND_ID_HANDLER(ID_VIEW_EMPTYTITLEWINDOWS, OnToggleEmptyTitleWindows)
		MESSAGE_HANDLER(WM_LOCATE_WINDOW, OnLocateWindow)
		if (uMsg == WM_COMMAND && WindowCommands::IsWindowCommand(LOWORD(wParam))) {
			// on the list's selected row when it has the focus, otherwise on the tree's
			WindowCommands::Execute(LOWORD(wParam), GetTargetWindow(), GetFrame());
			lResult = 0;
			return TRUE;
		}
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefreshAll)
		CHAIN_MSG_MAP(CTreeViewManager<CWindowsView>)
		CHAIN_MSG_MAP(CViewBase<CWindowsView>)
		if (m_WindowsView) {
			CHAIN_MSG_MAP_ALT_MEMBER(m_WindowsView, 1)
		}
	END_MSG_MAP()

	LRESULT OnTreeNodeRightClick(HTREEITEM hItem, CPoint const& pt);

private:
	// IWindowEventSink
	void OnWindowEvent(DWORD event, HWND hWnd) override;

	void UpdateUI();
	HWND GetTargetWindow() const;
	void InitTree();
	// the windows that match the search text, with their ancestors
	void InitSearchTree();
	bool MatchesSearch(HWND hWnd) const;
	void AddChildWindows(HTREEITEM hParent);
	// force: even if the View filters (hidden, no title) would leave it out
	CTreeItem AddNode(HWND hWnd, HTREEITEM hParent, bool force = false);
	bool PassesFilter(HWND hWnd) const;
	static CString GetNodeText(HWND hWnd);
	// the changes the window events told about since the last time
	void ProcessWindowEvents();
	void UpdateWindowNode(HWND hWnd);
	BOOL AddChildNode(HWND hChild);
	void NodeSelected();
	CTreeItem AddMessageOnlyWindows();
	void EnsureChildren(HTREEITEM hItem);
	LocateResult LocateWindow(HWND hWnd);
	LRESULT OnLocateWindow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSearchChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeDeleted(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeSelected(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRefreshAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleHiddenWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleEmptyTitleWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleChildWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	enum { MessageOnlyWindowsNode = 1 };
	enum { TimerSearch = 5, TimerEvents = 4, TimerKeyboardSelect = 3 };
	// a search shows at most this many matching windows
	static constexpr int MaxSearchResults = 2000;

	CCustomSplitterWindow m_Splitter;
	CWindowsListView m_WindowsView;
	//CTreeViewFrame m_TreeFrame;
	CTreeViewCtrlEx m_Tree;
	CTreeItem m_hCurrentNode;
	CTreeItem m_DesktopNode;
	CTreeItem m_Selected;
	CTreeItem m_MsgOnlyNode;
	CWindow m_SelectedHwnd;
	std::unordered_map<HWND, HTREEITEM> m_WindowMap;
	DWORD m_TotalWindows, m_TotalVisibleWindows, m_TopLevelWindows;
	CEdit m_Search;
	CString m_SearchText;		// upper case
	std::unordered_set<HWND> m_EventsDestroyed, m_EventsChanged;

	bool m_ShowHiddenWindows : 1 { false };
	bool m_ShowNoTitleWindows : 1 { true };
	bool m_ShowChildWindows : 1 { true };
	bool m_Deleting{ false };
};
