// WinSpy.cpp : main source file for WinSpy.exe
//

#include "pch.h"
#include "resource.h"
#include "MainFrm.h"
#include <WTLHelper.h>
#include "AppSettings.h"

CAppModule _Module;
AppSettings g_Settings;

int Run(LPCTSTR /*lpstrCmdLine*/ = nullptr, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	//
	// Always On Top is a creation style rather than a SetWindowPos(HWND_TOPMOST) later: Windows ignores that call
	// from a process that was started without being brought to the foreground (by a script, for instance)
	//
	DWORD exStyle = CMainFrame::GetWndExStyle(0) | (AppSettings::Get().AlwaysOnTop() ? WS_EX_TOPMOST : 0);
	if (wndMain.CreateEx(nullptr, nullptr, 0, exStyle) == nullptr) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	// where the window was when WinSpy was last closed, if that place is still on a screen
	auto wp = AppSettings::Get().MainWindowPlacement();
	if (wp.length == sizeof(wp) && ::MonitorFromRect(&wp.rcNormalPosition, MONITOR_DEFAULTTONULL)) {
		if (wp.showCmd == SW_SHOWMINIMIZED)
			wp.showCmd = SW_SHOWNORMAL;
		wndMain.SetWindowPlacement(&wp);
	}
	else {
		wndMain.ShowWindow(nCmdShow);
	}

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow) {
	// per-monitor (v2) DPI aware, so the coordinates WinSpy shows and works with are the real ones;
	// set here rather than in the manifest, as the project's DPI awareness option offers only the first version
	::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HRESULT hRes = ::CoInitialize(nullptr);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES);

	hRes = _Module.Init(nullptr, hInstance);
	ATLASSERT(SUCCEEDED(hRes));
	AppSettings::Get().Load(AppSettings::RegistryKey);
	// dark or not as the user last chose, and as the system is until then
	int dark = AppSettings::Get().DarkMode();
	if (dark < 0)
		WTLHelper::InitDarkMode();
	else
		WTLHelper::InitDarkMode(dark ? DarkModeKind::Dark : DarkModeKind::Classic);

	int nRet = Run(lpstrCmdLine, nCmdShow);
	// now: the views keep their state as they are destroyed, which is after the main window's WM_DESTROY
	AppSettings::Get().Save();

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
