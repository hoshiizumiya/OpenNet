#pragma once

struct SettingsPageTag
{
	std::wstring_view TypeName;
	std::wstring_view Route;
	std::wstring_view TagsResourceKey;

	static std::vector<SettingsPageTag>& Registry();
};
