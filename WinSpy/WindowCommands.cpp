#include "pch.h"
#include "resource.h"
#include "WindowCommands.h"
#include "WindowHelper.h"
#include "Interfaces.h"

bool WindowCommands::IsWindowCommand(UINT id) {
	switch (id) {
		case ID_WINDOW_SHOW:
		case ID_WINDOW_HIDE:
		case ID_WINDOW_MINIMIZE:
		case ID_WINDOW_MAXIMIZE:
		case ID_WINDOW_RESTORE:
		case ID_WINDOW_BRINGTOFRONT:
		case ID_TREE_SENDTOBACK:
		case ID_STATE_CLOSE:
		case ID_STATE_FLASH:
		case ID_WINDOW_HIGHLIGHT:
		case ID_WINDOW_MESSAGES:
		case ID_WINDOW_PROPERTIES:
		case ID_WINDOW_LOCATEINTREE:
			return true;
	}
	return false;
}

void WindowCommands::Execute(UINT id, HWND hWnd, IMainFrame* frame) {
	if (!::IsWindow(hWnd)) {
		::MessageBeep(MB_ICONWARNING);
		return;
	}

	CWindow win(hWnd);
	switch (id) {
		case ID_WINDOW_SHOW: win.ShowWindowAsync(SW_SHOW); break;
		case ID_WINDOW_HIDE: win.ShowWindowAsync(SW_HIDE); break;
		case ID_WINDOW_MINIMIZE: win.ShowWindowAsync(SW_MINIMIZE); break;
		case ID_WINDOW_MAXIMIZE: win.ShowWindowAsync(SW_MAXIMIZE); break;
		case ID_WINDOW_RESTORE: win.ShowWindowAsync(SW_RESTORE); break;
		case ID_WINDOW_BRINGTOFRONT: win.BringWindowToTop(); break;
		case ID_TREE_SENDTOBACK: win.SetWindowPos(HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); break;
		case ID_STATE_CLOSE: win.PostMessage(WM_CLOSE); break;
		case ID_STATE_FLASH: WindowHelper::Flash(hWnd); break;
		case ID_WINDOW_HIGHLIGHT: WindowHelper::HighlightWindow(hWnd); break;
		case ID_WINDOW_MESSAGES: frame->CaptureMessages(hWnd); break;
		case ID_WINDOW_PROPERTIES: WindowHelper::ShowWindowProperties(hWnd); break;
		case ID_WINDOW_LOCATEINTREE: frame->LocateInWindowsTree(hWnd); break;
	}
}
