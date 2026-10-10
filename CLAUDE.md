# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

WinSpy is a work-in-progress, improved version of the classic Spy++ tool. It is a native Win32 C++20 GUI app built with ATL/WTL, with no exceptions (`_HAS_EXCEPTIONS 0`, `ExceptionHandling=false`). There are no tests.

## Build

This is a Visual Studio 2026 solution (`WinSpy.sln`, toolset v145) with three projects:

- **WinSpy**: the main EXE.
- **WinSpyHook**: the hook DLL.
- **WTLHelper**: a git submodule (`github.com/zodiacon/WTLHelper`) holding reusable WTL controls such as `CustomTabView` and `OwnerDrawnMenu`. WinSpy links it as a static lib. It is still on toolset v143, so VS 2022 build tools must also be installed. Run `git submodule update --init` after cloning.

The WTL headers come from vcpkg (the `wtl` port with user-wide `vcpkg integrate install`). They are not vendored. The `..\WTL` entry in `IncludePath` is stale.

Configurations are `Debug`, `Release` and `ReleaseSigned`. Platforms are `x64` and `x86`/Win32; ARM64 maps to x64. To build from a VS Developer prompt:

```
msbuild WinSpy.sln -p:Configuration=Debug -p:Platform=x64 -m
```

The solution doesn't declare a project dependency between WinSpy and WinSpyHook, because WinSpy loads the DLL at runtime. Build the whole solution, or both projects, so that `WinSpyHook.dll` ends up next to `WinSpy.exe` in `x64\<Config>\`.

## Architecture

**Main frame + tabbed views.** `CMainFrame` (`MainFrm.*`) hosts a `CNativeCustomTabView` from WTLHelper. Each tab is a view, such as `CWindowsView`, `CProcessesView`, `CAllWindowsView` (View ▸ Windows List), `CAutomationTreeView` or `CMessagesView`, and derives from `CViewBase<T>` (`ViewBase.h`). That CRTP base provides:
- a per-view toolbar (`CreateAndInitToolBar` with `ToolBarButtonInfo[]`) wired into `CAutoUpdateUI` and the idle handler;
- `OnActivate(bool)` on tab switch (`TBVN_PAGEACTIVATED`);
- self-deletion in `OnFinalMessage`, so views are heap-allocated and owned by their HWND.

Views talk back to the frame only through the `IMainFrame` interface (`Interfaces.h`), which includes `CaptureMessages` and `LocateInWindowsTree`. `CMainFrame` forwards any command it doesn't handle to the active view (`OnCommandToActiveView`), so view-specific menu and toolbar commands are handled in the view's own message map. Add tabs with `CMainFrame::AddView`, which applies the Options ▸ Fonts font to the tab's trees and lists. Edit ▸ Copy is handled by the frame for whichever tree or list has the focus.

**Window commands.** Show, Hide, Close, Flash, Highlight, Send to Back, Messages, Properties, Locate in Tree and the like all go through `WindowCommands::Execute` (`WindowCommands.*`). Each view only decides which window is the target:
- `CWindowsView` uses `GetTargetWindow`: the list's selected row when the list has the focus, otherwise the tree's window.
- `CProcessesView` handles them when its tree has the focus, and otherwise lets the list handle them.
- `CWindowsListView`'s `ALT_MSG_MAP(1)` (the list pane, chained by its host view) acts on its selected row.

The list pane remembers its mode (window, thread, process or all) so `Refresh` repeats the last query. Locate in Tree sends `WM_LOCATE_WINDOW` to each tab; `CWindowsView` answers it.

**Dark mode** works the same way as in AstroStudio, through WTLHelper's DarkMode library:
- **Startup:** `WinMain` loads `AppSettings` (`HKCU\Software\WinSpy\Settings`, or `WinSpy.ini` next to the EXE) and calls `WTLHelper::InitDarkMode` before any window is created. `DarkMode` is -1 until the user chooses, which means follow the system.
- **Toggle:** Options ▸ Dark Mode (`ID_OPTIONS_DARKMODE`) calls `WTLHelper::SwitchToMode`, which re-themes every child and sends `WTLHelper::ThemeChangedMessage` to all descendants. Settings are saved in `CMainFrame::OnDestroy`.
- **How it themes:** WTLHelper themes windows as they're created, using a thread hook, and detours `GetSysColor`/`GetSysColorBrush`/`FillRect`. That's why WinSpy links Detours, and why the linker warns about `detours.lib`. In custom drawing, use system colors or check `WTLHelper::IsDarkMode()`; don't hard-code light colors.
- **Menus:** they are plain Win32 menus, not owner-drawn. Icons are set as item bitmaps by `CMainFrame::InitMenu` → `WTLHelper::InitMenu`. Those bitmaps are drawn on the current background, so `InitMenu` runs again after a mode switch, and on every context menu shown through `IMainFrame::ShowPopupMenu`. Show context menus through `ShowPopupMenu` so they get icons.

**Tree views** mix in `CTreeViewManager<T>` (`TreeViewManager.h`). It turns `NM_RCLICK`/`NM_DBLCLK` into `OnTreeNodeRightClick` / `OnTreeNodeDoubleClick` overridables. Chain its message map in the view.

**Message spying (hooks).** `WinSpyHook.dll` installs `SetWindowsHookEx` hooks into the target thread:
- **Exports:** `AddHook` and `RemoveHook`, declared in `WinSpyHook/hooks.h` and shared with the EXE through the include path.
- **Hook procedures:** named `HookFunc<hookType>`, for example `HookFunc3` for `WH_GETMESSAGE`. They are looked up by name with `GetProcAddress`, so a new hook type needs both the function and a `PRIVATE` export in `WinSpyHook.def`.
- **Shared state:** hook registrations live in a shared data section (`#pragma data_seg(".shared")`, linked RWS), so every injected process can see them.
- **Three hooks per thread:** `WH_GETMESSAGE` (posted), `WH_CALLWNDPROC` (sent) and `WH_CALLWNDPROCRET` (returned, with the result). Each capture adds one hook of each type per thread; a process capture covers every UI thread. Slots: `MaxHooks` per type.
- **Pointer data:** a sent message's pointer parameter means nothing outside the target, so the hook copies what it points to into `CallWndProcData::Extra` (text, `WINDOWPOS`, `NMHDR`, `STYLESTRUCT`, `MINMAXINFO`, `CREATESTRUCT` and rectangles). The copy is guarded by `__try`/`__except`. `MessageDecoder::DecodeExtra` formats it.
- **Delivery:** the hook procedure sends captured data back to the spying window with `WM_COPYDATA`, using `SendMessageTimeout` so a stalled WinSpy can't hang the target. The payload is a struct that starts with `HookDataHeader`. `CHookCallbackWnd` validates sizes before `CMessagesView` uses it. The callback window runs on its own thread; `StopCapture` removes the hooks first, then waits for that thread to exit.
- **Thread flag:** the call-wnd-proc hooks' `wParam` is nonzero when the message came from *another* thread. That's the opposite of what the documentation says; it was verified by testing (`CallWndProcData::FromOtherThread`).
- **Filtering:** `MessageFilter` (kind, category, exclude and include lists) decides what `CMessagesView` shows. The last filter is saved in `AppSettings`. It is applied in two places:
  - **In the target:** `CMessagesView::UpdateHookFilter` passes it to the DLL via `SetHookFilter`, as a bit per message below `WM_USER` plus one flag for the rest. Excluded messages are never sent, which saves the target a cross-process call each. They are also never captured, so loosening the filter later only affects new messages.
  - **In the view:** the exact filter plus the search box select the rows to show (`m_Visible`).
- **Toolbar:** Pause (the callback drops messages), Clear, Auto Scroll, Save (CSV of the shown rows) and the search box (its own rebar band). At most `MaxMessages` are kept; beyond that the oldest are dropped.
- **Own process:** WinSpy refuses to capture its own messages, because they would feed back into the capture.
- **Loading:** the EXE loads the DLL dynamically through `HookHelper` (`LoadLibrary("WinSpyHook.dll")`).
- **Bitness:** hooks only inject into processes with the same bitness as the DLL.

**UI Automation.** `CAutomationTreeView` builds its tree with `IUIAutomation`/`IUIAutomationTreeWalker`, and fills child elements lazily as nodes expand.

**Helpers.** Most Win32 querying and formatting is done in static helper "classes" (`struct X final` with only static members): `WindowHelper`, `ProcessHelper`, `FormatHelper`, `SecurityHelper`, plus `MessageDecoder` for decoding messages. Put new utility code in these helpers, not in the views. `IconHelper` comes from WTLHelper. Don't add a WinSpy class with the same name as a WTLHelper one, because it causes LNK2005 errors.
