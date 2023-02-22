#include "SettingsManager.h"
#include <algorithm>
#include <ranges>

SettingsManager::SettingsMap SettingsManager::_settings;

bool SettingsManager::add(SettingID id, JSMVariableBase *setting)
{
	return _settings.emplace(id, setting).second;
}


void SettingsManager::resetAllSettings()
{
	static constexpr auto callReset = [](SettingsMap::value_type &kvPair)
	{
		kvPair.second->reset();
	};
	static constexpr auto exceptions = [](SettingsMap::value_type &kvPair)
	{
		return kvPair.first != SettingID::JSM_DIRECTORY;
	};
	ranges::for_each(_settings | views::filter(exceptions), callReset);
}