#pragma once

#include <VirtualListView.h>
#include "Interfaces.h"
#include "WindowHelper.h"
#include "ProcessHelper.h"
#include "WindowCommands.h"
#include <unordered_set>

class CWindowsListView : 
	public CFrameWindowImpl<CWindowsListView, CWindow, CControlWinTraits>,
	public CVirtualListView<CWindowsListView>,
	public CCustomDraw<CWindowsListView> {
public:
	using BaseFrame = CFrameWindowImpl<CWindowsListView, CWindow, CControlWinTraits>;

	CWindowsListView(IMainFrame* frame) : m_pFrame(frame) {}

	HWND GetSelectedListHwnd() const;
	bool IsListed(HWND hWnd) const;
	// a process's integrity level, looked up once
	CString const& GetIntegrity(DWORD pid) const;
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	bool IsSortable(HWND, int col) const;
	void DoSort(const SortInfo* si);
	bool OnRightClickList(HWND, int row, int col, CPoint const&);
	bool OnDoubleClickList(HWND, int row, int col, CPoint const&);

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);

	void SetSelectedHwnd(HWND hWnd);
	void UpdateList(HWND hWnd);
	void UpdateListByThread(DWORD tid);
	void UpdateListByProcess(ProcessInfo const& pi);
	// all the top-level windows
	void UpdateListAll();
	void UpdateUI(CUpdateUIBase& ui);

	// does again what the last UpdateList* call did
	void Refresh();

	enum class DataItemType {
		Handle, ClassName, Text, Style, ExtendedStyle, ProcessName,
		ProcessId, ThreadId, ParentWindow, FirstChildWindow, NextWindow, PrevWindow, OwnerWindow,
		WindowProc, UserData, ID, Rectangle,
		ClassAtom, ClassStyle, ClassExtra, WindowExtra,
		DpiAwareness, Integrity, Cloaked,
	};
	struct DataItem {
		CString Property;
		DataItemType Type;
	};

protected:
	BEGIN_MSG_MAP(CWindowsListView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnItemChanged)
		CHAIN_MSG_MAP(CVirtualListView<CWindowsListView>)
		CHAIN_MSG_MAP(CCustomDraw<CWindowsListView>)
		CHAIN_MSG_MAP(BaseFrame)

	ALT_MSG_MAP(1)
		if (::GetFocus() != m_List)
			return FALSE;
		if (uMsg == WM_COMMAND && WindowCommands::IsWindowCommand(LOWORD(wParam))) {
			WindowCommands::Execute(LOWORD(wParam), GetSelectedListHwnd(), m_pFrame);
			lResult = 0;
			return TRUE;
		}
		COMMAND_ID_HANDLER(ID_VIEW_HIDDENWINDOWS, OnToggleHiddenWindows)
		COMMAND_ID_HANDLER(ID_VIEW_EMPTYTITLEWINDOWS, OnToggleEmptyTitleWindows)
		COMMAND_ID_HANDLER(ID_VIEW_CHILDWINDOWS, OnToggleChildWindows)
	END_MSG_MAP()

private:
	void AddChildWindows(std::vector<WindowItem>& v, HWND hParent, bool directOnly);
	void AddThreadWindows(DWORD tid);
	CString GetDetails(const DataItem& item) const;
	void UpdateList();
	void AddMessageOnlyWindows();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleHiddenWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleEmptyTitleWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleChildWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

private:
	enum class ListMode {
		Window,		// a window and its children
		Thread,
		Process,
		All,
	};

	CListViewCtrl m_List;
	ListMode m_Mode{ ListMode::Window };
	DWORD m_ModeThread{ 0 };
	ProcessInfo m_ModeProcess;
	std::vector<WindowItem> m_Items;
	mutable std::unordered_set<HWND> m_NoIcon;
	mutable std::unordered_map<DWORD, CString> m_Integrity;		// by process ID (opening a process on every repaint is costly)
	DWORD m_TotalWindows, m_TotalVisibleWindows, m_TopLevelWindows;
	CWindow m_SelectedHwnd;
	IMainFrame* m_pFrame;
	bool m_ShowHiddenWindows : 1 { false };
	bool m_ShowNoTitleWindows : 1 { true };
	bool m_ShowChildWindows : 1 { true };
	bool m_Deleting : 1{ false };
	bool m_ContextMenuOpen : 1{ false };
};
