#pragma once

#include "FormatHelper.h"

struct WindowItem {
	HWND hWnd;
	DWORD ThreadId;
	DWORD ProcessId;
	CString ProcessName;
};

struct StyleItem {
	DWORD Value;
	PCWSTR Text;
	DWORD Mask{ Value };
};

struct WindowHelper final {
	static CString WindowStyleToString(HWND hWnd);
	static CString ClassStyleToString(HWND hWnd);
	static CString WindowExtendedStyleToString(HWND hWnd);
	static CString WindowRectToString(HWND hWnd);
	static CString GetWindowClassName(HWND hWnd);
	static CString GetWindowText(HWND hWnd);
	static HICON GetWindowIcon(HWND hWnd);
	static HICON GetWindowOrProcessIcon(HWND hWnd);
	static bool Flash(HWND hWnd);
	// flashes a red border around the window a few times (without blocking)
	static void HighlightWindow(HWND hWnd);
	// the same around a rectangle on the screen (a UI Automation element's, say); the calling thread must pump messages
	static void HighlightRect(RECT const& rc);
	// a size in pixels at 96 DPI, for the main window's DPI (GetDpi)
	static int DpiScale(int size);
	// the DPI of the main window's monitor (the system's until SetDpi is called)
	static UINT GetDpi();
	// the main window is on a monitor with this DPI: the window icons are made again at its size (with the same
	// indices, so the trees and lists only need to be given the image list again)
	static void SetDpi(UINT dpi);
	// adds an icon to the window icons' image list (GetImageList); a copy is kept for making it again at another DPI
	static int AddIcon(HICON hIcon);
	// the UI (message) font for GetDpi(), for search boxes, toolbars, the tabs and the status bar
	static HFONT GetUIFont();
	// the rebar band of a toolbar made as big as all its buttons (after its images or font changed)
	static void FitToolBarBand(HWND hReBar, HWND hToolBar);
	// a list view's column widths (at 96 DPI) and order, as text (for the settings)
	static CString GetColumnsLayout(CListViewCtrl& list);
	// all the list views' columns under the window made wider or narrower, from one DPI to another
	static void ScaleColumns(HWND hRoot, UINT oldDpi, UINT newDpi);
	// ignored if the list's columns are not the ones the layout was made of
	static void SetColumnsLayout(CListViewCtrl& list, CString const& layout);
	// "Per-Monitor v2 (144)" and the like
	static CString GetDpiAwareness(HWND hWnd);
	// No, App, Shell or Inherited (DWM hides cloaked windows though they are "visible")
	static CString GetCloakedState(HWND hWnd);
	static void HighlightBorder(HWND hWnd, bool highlight = true);
	static CString GetWindowClassAndTitle(HWND hWnd);
	static std::unordered_map<HWND, int>& GetIconMap();
	// forgets destroyed windows, as their handles can be reused by new ones
	static void PruneIconMap();
	static CImageList& GetImageList();
	static WindowItem GetWindowInfo(HWND hWnd);
	static bool ThreadHasWindows(DWORD tid);
	static int ShowWindowProperties(HWND hWnd);
	static CString WindowMessageToString(DWORD msg);
	static ULONG_PTR GetID(HWND hWnd);
	static ULONG_PTR GetUserData(HWND hWnd);

	static std::pair<StyleItem const*, int> GetWindowStyleArray();
	// the styles of the control the window is (list view, button...), if it's one; empty otherwise.
	// defaultStyle: the name of the style that is 0, if there's one
	static std::pair<StyleItem const*, int> GetControlStyleArray(HWND hWnd, PCWSTR* defaultStyle = nullptr);
	// for the properties pages
	static PropertyList GetClassProperties(HWND hWnd);
	static PropertyList GetWindowProps(HWND hWnd);
	static PropertyList GetScrollProperties(HWND hWnd);
	static std::pair<StyleItem const*, int> GetListViewStyleArray();
	static std::pair<StyleItem const*, int> GetTreeViewStyleArray();
	static std::pair<StyleItem const*, int> GetTabCtrlStyleArray();
	static std::pair<StyleItem const*, int> GetWindowStyleExArray();
	static std::pair<StyleItem const*, int> GetClassStyleArray();
	static std::pair<StyleItem const*, int> GetEditStyleArray();
	static std::pair<StyleItem const*, int> GetListBoxStyleArray();
	static std::pair<StyleItem const*, int> GetComboBoxStyleArray();
	static std::pair<StyleItem const*, int> GetHeaderStyleArray();
	static std::pair<StyleItem const*, int> GetButtonStyleArray();
	static std::pair<StyleItem const*, int> GetStaticStyleArray();
	static std::pair<StyleItem const*, int> GetToolTipStyleArray();
	static std::pair<StyleItem const*, int> GetStatusBarStyleArray();
	static std::pair<StyleItem const*, int> GetToolBarStyleArray();
	static std::pair<StyleItem const*, int> GetRebarStyleArray();
	static std::pair<StyleItem const*, int> GetScrollBarStyleArray();
};

//
// windows being created, destroyed, shown, hidden, renamed or reparented anywhere (except in WinSpy itself)
//
struct IWindowEventSink {
	virtual void OnWindowEvent(DWORD event, HWND hWnd) = 0;
};

struct WindowEvents final {
	// the WinEvent hooks are set while there are sinks; the events come on this (the UI) thread, through its message loop
	static void Subscribe(IWindowEventSink* sink);
	static void Unsubscribe(IWindowEventSink* sink);

private:
	static void CALLBACK OnEvent(HWINEVENTHOOK, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD, DWORD);

	inline static std::vector<IWindowEventSink*> s_Sinks;
	inline static std::vector<HWINEVENTHOOK> s_Hooks;
};
