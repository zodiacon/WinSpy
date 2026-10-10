#pragma once

#include "ViewBase.h"
#include "WindowsListView.h"
#include "TreeViewManager.h"
#include "ProcessHelper.h"
#include "WindowCommands.h"

class CProcessesView : 
	public CViewBase<CProcessesView>,
	public CTreeViewManager<CProcessesView>,
	public IWindowEventSink {
public:
	CProcessesView(IMainFrame* frame) : CViewBase(frame), m_WindowsView(frame) {}

	void OnActivate(bool active);

protected:
	enum { IDC_TREE = 123 };

	BEGIN_MSG_MAP(CProcessesView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		NOTIFY_CODE_HANDLER(TVN_DELETEITEM, OnNodeDeleted)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
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

	// IWindowEventSink
	void OnWindowEvent(DWORD event, HWND hWnd) override;

	void InitTree();
	void UpdateUI();
	CTreeItem AddProcessNode(ProcessInfo const& pi);
	CTreeItem AddThreadNode(HTREEITEM hProcess, DWORD pid, DWORD tid, int icon);
	// the thread's node, made (with its process's node) if it's not there
	HTREEITEM EnsureThreadNode(DWORD pid, DWORD tid);
	CTreeItem AddNode(HWND hWnd, HTREEITEM hParent);
	bool PassesFilter(HWND hWnd) const;
	// the thread a window's node is under
	DWORD GetNodeThread(HTREEITEM hItem) const;
	// the changes the window events told about since the last time
	void ProcessWindowEvents();
	void UpdateWindowNode(HWND hWnd, std::unordered_set<DWORD>& threads);
	// a thread whose windows are all gone goes, and its process with it if it was the last
	void RemoveThreadIfEmpty(DWORD tid);
	void AddChildWindows(HTREEITEM hParent);
	BOOL AddChildNode(HWND hChild);
	void ChangeSelection(HTREEITEM hItem);

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
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

	enum { TimerKeyboardSelect = 3, TimerEvents = 4 };

	CWindowsListView m_WindowsView;
	CSplitterWindow m_Splitter;
	CTreeViewCtrlEx m_Tree;

	struct ThreadNode {
		HTREEITEM Node;
		DWORD ProcessId;
	};
	std::unordered_map<HTREEITEM, ProcessInfo> m_Processes;
	std::unordered_map<HTREEITEM, DWORD> m_Threads;
	std::unordered_map<DWORD, HTREEITEM> m_ProcessNodes;	// by process ID
	std::unordered_map<DWORD, ThreadNode> m_ThreadNodes;	// by thread ID
	std::unordered_map<HWND, HTREEITEM> m_WindowMap;
	std::unordered_set<HWND> m_EventsDestroyed, m_EventsChanged;
	bool m_Deleting{ false };		// the whole tree is being cleared
	CTreeItem m_hCurrentNode;
	CTreeItem m_Selected;
	CWindow m_SelectedHwnd;
	int m_TotalWindows, m_TotalVisibleWindows, m_TopLevelWindows;
	bool m_ShowHiddenWindows : 1 { true };
	bool m_ShowNoTitleWindows : 1 { true };
	bool m_ShowChildWindows : 1 { true };
};
