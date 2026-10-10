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

**Main frame + tabbed views.** `CMainFrame` (`MainFrm.*`) hosts a `CNativeCustomTabView` from WTLHelper. Each tab is a view, such as `CWindowsView`, `CProcessesView`, `CAutomationTreeView` or `CMessagesView`, and derives from `CViewBase<T>` (`ViewBase.h`). That CRTP base provides:
- a per-view toolbar (`CreateAndInitToolBar` with `ToolBarButtonInfo[]`) wired into `CAutoUpdateUI` and the idle handler;
- `OnActivate(bool)` on tab switch (`TBVN_PAGEACTIVATED`);
- self-deletion in `OnFinalMessage`, so views are heap-allocated and owned by their HWND.

Views talk back to the frame only through the `IMainFrame` interface (`Interfaces.h`). `CMainFrame` forwards any command it doesn't handle to the active view (`OnCommandToActiveView`), so view-specific menu and toolbar commands are handled in the view's own message map.

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
- **Delivery:** the hook procedure sends captured data back to the spying window with `WM_COPYDATA`. The payload is a struct that starts with `HookDataHeader`. `CMessagesView` handles it.
- **Loading:** the EXE loads the DLL dynamically through `HookHelper` (`LoadLibrary("WinSpyHook.dll")`).
- **Bitness:** hooks only inject into processes with the same bitness as the DLL.

**UI Automation.** `CAutomationTreeView` builds its tree with `IUIAutomation`/`IUIAutomationTreeWalker`, and fills child elements lazily as nodes expand.

**Helpers.** Most Win32 querying and formatting is done in static helper "classes" (`struct X abstract final`): `WindowHelper`, `ProcessHelper`, `FormatHelper`, `SecurityHelper`, plus `MessageDecoder` for decoding messages. Put new utility code in these helpers, not in the views. `IconHelper` comes from WTLHelper. Don't add a WinSpy class with the same name as a WTLHelper one, because it causes LNK2005 errors.
