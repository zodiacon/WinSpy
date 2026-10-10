#pragma once

#include <Settings.h>

// What the program remembers between runs, in HKCU\Software\WinSpy\Settings (or in WinSpy.ini next to the program,
// if there is one - see Settings::Load). Loaded at startup and saved at the end (after all the windows are destroyed,
// as the views keep their state as they are destroyed).
struct AppSettings : Settings {
	static constexpr PCWSTR RegistryKey = L"Software\\WinSpy\\Settings";

	BEGIN_SETTINGS(AppSettings)
		SETTING(DarkMode, -1, SettingType::Int32);		// 1 dark, 0 light, -1 (never chosen): as the system is
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(ViewFont, LOGFONT{}, SettingType::Binary);		// the trees' and lists' font (empty face: the default one)
		SETTING_STRING(MessageFilter, L"");		// the Messages view's filter (MessageFilter::ToString); empty: everything
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(PropertiesSize, SIZE{}, SettingType::Binary);		// the window properties sheet's
		// the splitters, in percent of the view's width
		SETTING(WindowsSplitter, 35, SettingType::Int32);
		SETTING(ProcessesSplitter, 35, SettingType::Int32);
		SETTING(AutomationSplitter, 35, SettingType::Int32);
		// View > Hidden Windows / Empty Title Windows, in each kind of tree and in the lists
		SETTING(WindowsShowHidden, 0, SettingType::Bool);
		SETTING(WindowsShowNoTitle, 1, SettingType::Bool);
		SETTING(ProcessesShowHidden, 1, SettingType::Bool);
		SETTING(ProcessesShowNoTitle, 1, SettingType::Bool);
		SETTING(ListShowHidden, 0, SettingType::Bool);
		SETTING(ListShowNoTitle, 1, SettingType::Bool);
		// the columns' widths and order (WindowHelper::GetColumnsLayout); empty: the defaults
		SETTING_STRING(WindowsListColumns, L"");
		SETTING_STRING(MessagesColumns, L"");
	END_SETTINGS

	DEF_SETTING(DarkMode, int)
	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING(ViewFont, LOGFONT)
	DEF_SETTING_STRING(MessageFilter)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING(PropertiesSize, SIZE)
	DEF_SETTING(WindowsSplitter, int)
	DEF_SETTING(ProcessesSplitter, int)
	DEF_SETTING(AutomationSplitter, int)
	DEF_SETTING(WindowsShowHidden, int)
	DEF_SETTING(WindowsShowNoTitle, int)
	DEF_SETTING(ProcessesShowHidden, int)
	DEF_SETTING(ProcessesShowNoTitle, int)
	DEF_SETTING(ListShowHidden, int)
	DEF_SETTING(ListShowNoTitle, int)
	DEF_SETTING_STRING(WindowsListColumns)
	DEF_SETTING_STRING(MessagesColumns)
};
