#include "XamlWorkaround.h"
#include "UI/Xaml/View/Pages/SettingsPages/SettingsPageTag.h"

std::vector<SettingsPageTag>& SettingsPageTag::Registry()
{
	static std::vector<SettingsPageTag> registry;
	return registry;
}
