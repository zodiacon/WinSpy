#pragma once

#include "VirtualListView.h"
#include "Interfaces.h"
#include "WindowHelper.h"
#include "ProcessHelper.h"

class CWindowsListView : 
	public CFrameWindowImpl<CWindowsListView, CWindow, CControlWinTraits>,
	public CVirtualListView<CWindowsListView>,
	public CCustomDraw<CWindowsListView> {
public:
	using BaseFrame = CFrameWindowImpl<CWindowsListView, CWindow, CControlWinTraits>;

	CWindowsListView(IMainFrame* frame) : m_pFrame(frame) {}

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row) const;
	bool IsSortable(int col) const;
	void DoSort(const SortInfo* si);
	bool OnRightClickList(HWND, int row, int col, CPoint const&);

	DWORD OnPrePaint(int, LPNMCUSTOMDRAW cd);
	DWORD OnItemPrePaint(int, LPNMCUSTOMDRAW cd);

	void SetSelectedHwnd(HWND hWnd);
	void UpdateList(HWND hWnd);
	void UpdateListByThread(DWORD tid);
	void UpdateListByProcess(ProcessInfo const& pi);

	void Refresh();

	enum class DataItemType {
		Handle, ClassName, Text, Style, ExtendedStyle, ProcessName,
		ProcessId, ThreadId, ParentWindow, FirstChildWindow, NextWindow, PrevWindow, OwnerWindow,
		WindowProc, UserData, ID, Rectangle,
		ClassAtom, ClassStyle, ClassExtra, WindowExtra,
	};
	struct DataItem {
		CString Property;
		DataItemType Type;
	};

protected:
	BEGIN_MSG_MAP(CWindowsListView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CWindowsListView>)
		CHAIN_MSG_MAP(CCustomDraw<CWindowsListView>)
		CHAIN_MSG_MAP(BaseFrame)

	ALT_MSG_MAP(1)
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
	END_MSG_MAP()

private:
	void AddChildWindows(std::vector<WindowItem>& v, HWND hParent, bool directOnly);
	void AddThreadWindows(DWORD tid);
	CString GetDetails(const DataItem& item) const;
	void UpdateList();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnWindowShow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowHide(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowMinimize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowMaximize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowRestore(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowFlash(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleHiddenWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleEmptyTitleWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnToggleChildWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWindowBringToFront(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:

	CListViewCtrl m_List;
	std::vector<WindowItem> m_Items;
	DWORD m_TotalWindows, m_TotalVisibleWindows, m_TopLevelWindows;
	CWindow m_SelectedHwnd;
	IMainFrame* m_pFrame;
	bool m_ShowHiddenWindows : 1 { false };
	bool m_ShowNoTitleWindows : 1 { true };
	bool m_ShowChildWindows : 1 { true };
	bool m_Deleting : 1{ false };
	bool m_ContextMenuOpen : 1{ false };
};
