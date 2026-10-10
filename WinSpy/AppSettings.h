#pragma once

#include <Settings.h>

// What the program remembers between runs, in HKCU\Software\WinSpy\Settings (or in WinSpy.ini next to the program,
// if there is one - see Settings::Load). Loaded at startup and saved when the main window closes.
struct AppSettings : Settings {
	static constexpr PCWSTR RegistryKey = L"Software\\WinSpy\\Settings";

	BEGIN_SETTINGS(AppSettings)
		SETTING(DarkMode, -1, SettingType::Int32);		// 1 dark, 0 light, -1 (never chosen): as the system is
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(ViewFont, LOGFONT{}, SettingType::Binary);		// the trees' and lists' font (empty face: the default one)
		SETTING_STRING(MessageFilter, L"");		// the Messages view's filter (MessageFilter::ToString); empty: everything
	END_SETTINGS

	DEF_SETTING(DarkMode, int)
	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING(ViewFont, LOGFONT)
	DEF_SETTING_STRING(MessageFilter)
};
