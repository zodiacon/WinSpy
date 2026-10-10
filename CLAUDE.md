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

Views talk back to the frame only through the `IMainFrame` interface (`Interfaces.h`), which includes `CaptureMessages`, `LocateInWindowsTree` and `LocateInAutomationTree`. `CMainFrame` forwards any command it doesn't handle to the active view (`OnCommandToActiveView`), so view-specific menu and toolbar commands are handled in the view's own message map. Add tabs with `CMainFrame::AddView`, which applies the Options ▸ Fonts font to the tab's trees and lists. Edit ▸ Copy is handled by the frame for whichever tree or list has the focus.

**Window commands.** Show, Hide, Close, Flash, Highlight, Send to Back, Messages, Properties, Locate in Tree and the like all go through `WindowCommands::Execute` (`WindowCommands.*`). Each view only decides which window is the target:
- `CWindowsView` uses `GetTargetWindow`: the list's selected row when the list has the focus, otherwise the tree's window.
- `CProcessesView` handles them when its tree has the focus, and otherwise lets the list handle them.
- `CWindowsListView`'s `ALT_MSG_MAP(1)` (the list pane, chained by its host view) acts on its selected row.

The list pane remembers its mode (window, thread, process or all) so `Refresh` repeats the last query. Locate in Tree sends `WM_LOCATE_WINDOW` to each tab; `CWindowsView` answers it.

**Live updates.** `WindowEvents` (in `WindowHelper.*`) sets out-of-context `SetWinEventHook`s, skipping WinSpy's own process, while any `IWindowEventSink` is subscribed. It reports create, destroy, show, hide, name change and parent change for whole windows.
- `CWindowsView` batches the events (a short timer) and updates, adds or removes tree nodes. A collapsed node whose children haven't been added yet just gets its "+" placeholder.
- The list pane is refreshed only when the events touch what it shows. Nothing polls, so changes that raise no event (such as styles) show up on View ▸ Refresh or when the tab is activated again.
- `CAllWindowsView` refreshes on top-level window events.
- `CProcessesView` does the same for its process → thread → window tree. A new top-level window gets its thread and process nodes made if needed (`EnsureThreadNode`). A thread goes when its last window goes, and its process goes when its last thread does (the same rule as the initial build: UI threads only). Its maps (`m_Processes`, `m_Threads`, `m_ProcessNodes`, `m_ThreadNodes`, `m_WindowMap`) are kept in step by `TVN_DELETEITEM`.

**Tree search.** The search box (Ctrl+F) rebuilds the tree with only the matching windows (class, title or handle) plus their ancestors, all expanded, without lazy placeholders. WinSpy's own windows are left out of the results. Events then only update or remove nodes; they don't add new matches.

**Window properties sheet** (`WindowHelper::ShowWindowProperties`):
- **Pages:** General and Windows (`WindowGeneralPage.*`, `WindowWindowsPage.*`), plus Styles (`CWindowStylesPage`, in `WindowGeneralPage.*`). The rest are `CPropertyListPage`s (`PropertiesDlg.*`) filled by provider functions: `WindowHelper::GetClassProperties`/`GetWindowProps`/`GetScrollProperties` and `ProcessHelper::GetProcessProperties`/`GetThreadProperties`. The standalone process and thread dialogs reuse the same providers.
- **Resizing:** the sheet is WTLHelper's `CResizablePropertySheetImpl`, combined with `CIconPropertySheetImpl` for full-color tab icons. Its size is remembered (`PropertiesSize`). Each page lays itself out with WTL's `CDynamicDialogLayout`, from its `AFX_DIALOG_LAYOUT` resource in `WinSpy.rc`: move-X, move-Y, size-X and size-Y percentages per control, in the template's control order. Adding or reordering controls in a page template means updating its layout resource to match.
- **Live refresh:** pages refresh every second while showing, only updating what changed. The Styles page reloads only when the user has no unapplied edits.
- **Following window links:** when the Windows page follows a link to another window, it sends `WM_PAGE_WINDOW_CHANGED` (LPARAM: the window) to every page.
- **Style lists:** the shared style-array table for control-specific styles is `WindowHelper::GetControlStyleArray`.

**Settings.** `AppSettings` is saved by `WinMain` after the message loop ends, not in the frame's `WM_DESTROY`. The views store their state (splitter positions, column layouts via `WindowHelper::Get/SetColumnsLayout`) in their own `WM_DESTROY`, which runs after the frame's. Toggles are saved when changed, and the window placement in `CMainFrame::OnDestroy`.

**DPI.** WinSpy is per-monitor v2 DPI aware. It's set by `SetProcessDpiAwarenessContext` at the start of `WinMain`, because the project's manifest option only offers v1. Icons, image lists and toolbars are sized once for the system DPI (`WindowHelper::DpiScale`). The trees' and lists' font is made for the window's DPI and remade on `WM_DPICHANGED` (`CMainFrame::CreateViewFont`). The Fonts choice is stored at 96 DPI. The code targets Windows 10 (`_WIN32_WINNT 0x0A00` in `pch.h`).

**Always On Top.** Windows ignores `SetWindowPos(HWND_TOPMOST)` from a process that was started without being brought to the foreground (by a script, say). This was verified with a plain Win32 window, so it isn't WTL's or WTLHelper's doing. That's why the saved setting is applied as `WS_EX_TOPMOST` when the frame is created (`Run` in `WinSpy.cpp`), which Windows always honors. The menu command uses `SetWindowPos`; that works because a click gives the process foreground rights.

**Locate under the cursor.** Ctrl+Alt+F12 is a global hot key (`RegisterHotKey`, `CMainFrame::OnHotKey`) that locates the window under the mouse in a windows tree. When the active tab is a UI Automation tree, it locates the element under the mouse there instead.

**Dark mode** works the same way as in AstroStudio, through WTLHelper's DarkMode library:
- **Startup:** `WinMain` loads `AppSettings` (`HKCU\Software\WinSpy\Settings`, or `WinSpy.ini` next to the EXE) and calls `WTLHelper::InitDarkMode` before any window is created. `DarkMode` is -1 until the user chooses, which means follow the system.
- **Toggle:** Options ▸ Dark Mode (`ID_OPTIONS_DARKMODE`) calls `WTLHelper::SwitchToMode`, which re-themes every child and sends `WTLHelper::ThemeChangedMessage` to all descendants.
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

**UI Automation.** `CAutomationTreeView` builds its tree with `IUIAutomation` and the raw view `IUIAutomationTreeWalker`, and fills child elements lazily as nodes expand.
- **Locating:** `WM_LOCATE_ELEMENT` (`Interfaces.h`) selects an element by window (Locate in Automation Tree, a window command, through `IMainFrame::LocateInAutomationTree`) or by screen point (Ctrl+Alt+F12 when this tab is the active one). It walks the element's ancestors from the root and enumerates each level's children if needed. Expanding a node by code notifies only the first time, so it doesn't rely on `TVN_ITEMEXPANDING`. A level whose children were enumerated before the element existed is enumerated again.
- **Properties:** the list shows the element's properties, its supported patterns, and their state (value, toggle, expand/collapse, selection, range, scroll, window, default action).
- **Actions:** the toolbar and the tree's context menu run pattern actions on the selected element (Invoke, Toggle, Expand/Collapse, Select, Set Value, Default Action, Set Focus, Scroll Into View). They are enabled by the patterns found when it was selected (`ElementPatterns`).
- **Window commands:** Highlight draws a flashing frame around the element's bounding rectangle (`WindowHelper::HighlightRect`, a click-through layered window). The other window commands act on the element's window, or its nearest ancestor's that has one.

**Helpers.** Most Win32 querying and formatting is done in static helper "classes" (`struct X final` with only static members): `WindowHelper`, `ProcessHelper`, `FormatHelper`, `SecurityHelper`, plus `MessageDecoder` for decoding messages. Put new utility code in these helpers, not in the views. `IconHelper` comes from WTLHelper. Don't add a WinSpy class with the same name as a WTLHelper one, because it causes LNK2005 errors.
