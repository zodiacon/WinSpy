#pragma once

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

struct WindowHelper abstract final {
	static CString WindowStyleToString(HWND hWnd);
	static CString ClassStyleToString(HWND hWnd);
	static CString WindowExtendedStyleToString(HWND hWnd);
	static CString WindowRectToString(HWND hWnd);
	static CString GetWindowClassName(HWND hWnd);
	static CString GetWindowText(HWND hWnd);
	static HICON GetWindowIcon(HWND hWnd);
	static HICON GetWindowOrProcessIcon(HWND hWnd);
	static bool Flash(HWND hWnd);
	static void HighlightBorder(HWND hWnd, bool highlight = true);
	static CString GetWindowClassAndTitle(HWND hWnd);
	static std::unordered_map<HWND, int>& GetIconMap();
	static CImageList& GetImageList();
	static WindowItem GetWindowInfo(HWND hWnd);
	static bool ThreadHasWindows(DWORD tid);
	static int ShowWindowProperties(HWND hWnd);
	static CString WindowMessageToString(DWORD msg);

	static std::pair<StyleItem const*, int> GetWindowStyleArray();
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
};


