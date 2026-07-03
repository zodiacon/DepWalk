#pragma once

#include <Settings.h>

class AppSettings : public Settings {
public:
	BEGIN_SETTINGS(AppSettings)
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(Font, LOGFONT{}, SettingType::Binary);
	END_SETTINGS

	DEF_SETTING(DarkMode, int)
	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING_MULTI(RecentFiles)
	DEF_SETTING(Font, LOGFONT)
};
