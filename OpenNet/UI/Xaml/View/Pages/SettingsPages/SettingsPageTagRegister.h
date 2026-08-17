#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/SettingsPageTag.h"

template<typename PageImplementation>
struct SettingsPageTagRegister
{
	SettingsPageTagRegister(
		std::wstring_view const route,
		std::wstring_view const tagsResourceKey)
	{
		SettingsPageTag::Registry().push_back({
			winrt::name_of<typename PageImplementation::class_type>(),
			route,
			tagsResourceKey });
	}
};
