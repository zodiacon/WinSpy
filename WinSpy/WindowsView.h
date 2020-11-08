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
	using Base = CFrameWindowImpl<CWindowsView, CWindow, CControlWinTraits>;

	BOOL PreTranslateMessage(MSG* pMsg);

	CString GetColumnText(HWND, int row, int col) const;

	virtual void OnFinalMessage(HWND /*hWnd*/);

	BEGIN_MSG_MAP(CWindowsView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnNodeSelected)
		CHAIN_MSG_MAP(CVirtualListView<CWindowsView>)
		CHAIN_MSG_MAP(CViewBase<CWindowsView>)
	END_MSG_MAP()

private:
	enum class DataItemType {
		Handle, ClassName, Text, TextLength, Style, ExtendedStyle,
		ProcessId, ThreadId, ParentWindow, FirstChildWindow, NextWindow, PrevWindow, OwnerWindow, 
	};
	struct WindowDataItem {
		CString Property;
		DataItemType Type;
	};

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
	LRESULT OnNodeExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeSelected(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

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
};
