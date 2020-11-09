// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VirtualListView.h"
#include "ViewBase.h"

class CWindowsView : 
	public CViewBase<CWindowsView>,
	public CVirtualListView<CWindowsView> {
public:
	CWindowsView(IMainFrame* frame) : CViewBase(frame) {}

	BOOL PreTranslateMessage(MSG* pMsg);

	CString GetColumnText(HWND, int row, int col) const;
	void OnActivate(bool activate);

	virtual void OnFinalMessage(HWND /*hWnd*/);

	enum { IDC_TREE = 123 };

	BEGIN_MSG_MAP(CWindowsView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnNodeSelected)
		NOTIFY_HANDLER(IDC_TREE, NM_RCLICK, OnTreeNodeRightClick)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_WINDOW_SHOW, OnWindowShow)
		COMMAND_ID_HANDLER(ID_WINDOW_HIDE, OnWindowHide)
		COMMAND_ID_HANDLER(ID_WINDOW_BRINGTOFRONT, OnWindowBringToFront)
		COMMAND_ID_HANDLER(ID_WINDOW_MINIMIZE, OnWindowMinimize)
		COMMAND_ID_HANDLER(ID_WINDOW_MAXIMIZE, OnWindowMaximize)
		COMMAND_ID_HANDLER(ID_STATE_FLASH, OnWindowFlash)
		COMMAND_ID_HANDLER(ID_WINDOW_RESTORE, OnWindowRestore)
		COMMAND_ID_HANDLER(ID_VIEW_HIDDENWINDOWS, OnToggleHiddenWindows)
		COMMAND_ID_HANDLER(ID_VIEW_EMPTYTITLEWINDOWS, OnToggleEmptyTitleWindows)
		COMMAND_ID_HANDLER(ID_VIEW_CHILDWINDOWS, OnToggleChildWindows)
		CHAIN_MSG_MAP(CVirtualListView<CWindowsView>)
		CHAIN_MSG_MAP(CViewBase<CWindowsView>)
	END_MSG_MAP()

private:
	enum class DataItemType {
		Handle, ClassName, Text, Style, ExtendedStyle,
		ProcessId, ThreadId, ParentWindow, FirstChildWindow, NextWindow, PrevWindow, OwnerWindow, 
		WindowProc, UserData, ID, Rectangle,
	};
	struct WindowDataItem {
		CString Property;
		DataItemType Type;
	};

	void UpdateUI();
	void Refresh();
	void InitTree();
	void AddChildWindows(HTREEITEM hParent);
	CTreeItem AddNode(HWND hWnd, HTREEITEM hParent);
	BOOL AddChildNode(HWND hChild);
	void UpdateList();
	CString GetDetails(const WindowDataItem& item) const;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeSelected(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnWindowShow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowHide(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowMinimize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowMaximize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowRestore(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleHiddenWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleEmptyTitleWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleChildWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTreeNodeRightClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnWindowFlash(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowBringToFront(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CSplitterWindow m_Splitter;
	CListViewCtrl m_List;
	CTreeViewCtrlEx m_Tree;
	CImageList m_Images;
	CTreeItem m_hCurrentNode;
	CTreeItem m_DesktopNode;
	CTreeItem m_Selected;
	CWindow m_SelectedHwnd;
	std::vector<WindowDataItem> m_Items;
	bool m_ShowHiddenWindows : 1 { false };
	bool m_ShowNoTitleWindows : 1 { true };
	bool m_ShowChildWindows : 1 { true };
};
