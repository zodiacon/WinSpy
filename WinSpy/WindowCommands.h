#pragma once

struct IMainFrame;

//
// the commands that act on a window (Show, Hide, Close, Messages, Properties...), for the views that show windows
//
struct WindowCommands final {
	static bool IsWindowCommand(UINT id);
	static void Execute(UINT id, HWND hWnd, IMainFrame* frame);
};
