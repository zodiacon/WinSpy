#pragma once

//
// sent by the frame to its pages: a windows tree selects the window (LPARAM) and returns LocateResult
//
const UINT WM_LOCATE_WINDOW = WM_APP + 100;

enum class LocateResult {
	NotHandled,		// not a windows tree
	Found,
	NotInTree,		// e.g. a hidden window while hidden windows are not shown
};

//
// sent by the frame to its pages: a UI Automation tree selects an element and returns LocateResult
// WPARAM: LocateElementByWindow (LPARAM: the HWND) or LocateElementByPoint (LPARAM: a POINT*, in screen coordinates)
//
const UINT WM_LOCATE_ELEMENT = WM_APP + 101;
enum { LocateElementByWindow, LocateElementByPoint };

struct IMainFrame {
	virtual CUpdateUIBase& GetUIUpdate() = 0;
	virtual UINT ShowPopupMenu(HMENU hMenu, const POINT& pt, DWORD flags = 0) = 0;
	virtual HWND GetHwnd() const = 0;
	// in a new tab: the messages of a window, a thread or a process (the first one given)
	virtual bool CaptureMessages(HWND hWnd, DWORD threadId = 0, DWORD processId = 0) = 0;
	// selects the window in a windows tree tab (made if there is none)
	virtual void LocateInWindowsTree(HWND hWnd) = 0;
	// selects the window's element in a UI Automation tree tab (made if there is none)
	virtual void LocateInAutomationTree(HWND hWnd) = 0;
	virtual void CloseTab(CWindow* win) = 0;
};

struct ToolBarButtonInfo {
	UINT id;
	int image;
	BYTE style = BTNS_BUTTON;
	PCWSTR text = nullptr;
};
