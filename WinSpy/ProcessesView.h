#pragma once

#include "ViewBase.h"
#include "WindowsListView.h"
#include "TreeViewManager.h"
#include "ProcessHelper.h"
#include "WindowCommands.h"

class CProcessesView : 
	public CViewBase<CProcessesView>,
	public CTreeViewManager<CProcessesView> {
public:
	CProcessesView(IMainFrame* frame) : CViewBase(frame), m_WindowsView(frame) {}

	void OnActivate(bool active);

protected:
	enum { IDC_TREE = 123 };

	BEGIN_MSG_MAP(CProcessesView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		//NOTIFY_CODE_HANDLER(TVN_DELETEITEM, OnNodeDeleted)
		COMMAND_ID_HANDLER(ID_VIEW_HIDDENWINDOWS, OnToggleHiddenWindows)
		COMMAND_ID_HANDLER(ID_VIEW_EMPTYTITLEWINDOWS, OnToggleEmptyTitleWindows)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnNodeSelected)
		COMMAND_ID_HANDLER(ID_PROCESS_PROPERTIES, OnProcessProperties)
		COMMAND_ID_HANDLER(ID_THREAD_PROPERTIES, OnThreadProperties)
		COMMAND_ID_HANDLER(ID_PROCESS_TERMINATE, OnTerminateProcess)
		COMMAND_ID_HANDLER(ID_WINDOW_MESSAGES, OnCaptureMessages)
		// the window commands act on the tree's window when it has the focus, otherwise the list handles them
		if (uMsg == WM_COMMAND && ::GetFocus() == m_Tree && WindowCommands::IsWindowCommand(LOWORD(wParam))) {
			WindowCommands::Execute(LOWORD(wParam), m_SelectedHwnd, GetFrame());
			lResult = 0;
			return TRUE;
		}
		CHAIN_MSG_MAP(CTreeViewManager<CProcessesView>)
		CHAIN_MSG_MAP(CViewBase<CProcessesView>)
		if (m_WindowsView) {
			CHAIN_MSG_MAP_ALT_MEMBER(m_WindowsView, 1)
		}
	END_MSG_MAP()

	LRESULT OnTreeNodeRightClick(HTREEITEM hItem, CPoint const& pt);
	LRESULT OnTreeNodeDoubleClick(HTREEITEM hItem, CPoint const& pt);

private:
	enum class ItemType {
		Process,
		Thread,
		Window
	};

	void InitTree();
	void RemoveFromWindowMap(HTREEITEM hItem);
	void UpdateUI();
	CTreeItem AddNode(HWND hWnd, HTREEITEM hParent);
	void AddChildWindows(HTREEITEM hParent);
	BOOL AddChildNode(HWND hChild);
	void ChangeSelection(HTREEITEM hItem);

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeDeleted(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeSelected(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnToggleHiddenWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleEmptyTitleWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleChildWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnProcessProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnThreadProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTerminateProcess(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCaptureMessages(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// the process / thread of the selected node (or of the node it is under)
	ProcessInfo const* GetSelectedProcess() const;
	DWORD GetSelectedThread() const;

	CWindowsListView m_WindowsView;
	CSplitterWindow m_Splitter;
	CTreeViewCtrlEx m_Tree;

	std::unordered_map<HTREEITEM, ProcessInfo> m_Processes;
	std::unordered_map<HTREEITEM, DWORD> m_Threads;
	std::unordered_map<HWND, HTREEITEM> m_WindowMap;
	CTreeItem m_hCurrentNode;
	CTreeItem m_Selected;
	CWindow m_SelectedHwnd;
	int m_TotalWindows, m_TotalVisibleWindows, m_TopLevelWindows;
	bool m_ShowHiddenWindows : 1 { true };
	bool m_ShowNoTitleWindows : 1 { true };
	bool m_ShowChildWindows : 1 { true };
};
