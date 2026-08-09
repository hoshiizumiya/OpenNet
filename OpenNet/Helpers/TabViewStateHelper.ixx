/*
 * PROJECT:   OpenNet
 * FILE:      Helpers/TabViewStateHelper.ixx
 * PURPOSE:   Save / restore TabView state via AppSettingsDatabase.
 */

export module OpenNet.Helpers.TabViewStateHelper;

import std;

import OpenNet.Core.AppSettingsDatabase;

import winrt.Windows.Foundation;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;

export namespace OpenNet::Helpers::TabViewStateHelper
{
	using namespace winrt;
	using namespace winrt::Microsoft::UI::Xaml;
	using namespace winrt::Microsoft::UI::Xaml::Controls;

	inline constexpr std::string_view StateCategory = "TabViewState";

	namespace details
	{
		inline std::string MakeOrderKey(std::string const& key)
		{
			return key + ".Order";
		}

		inline std::string MakeSelectedKey(std::string const& key)
		{
			return key + ".Selected";
		}

		inline std::string MakeSettingKey(std::string const& key, std::string_view setting)
		{
			return key + ".Settings." + std::string{ setting };
		}

		inline hstring GetItemId(TabViewItem const& item)
		{
			if (!item)
				return {};

			/*
			 * x:Name serves as the persistent stable ID.
			 */
			return item.Name();
		}

		inline std::vector<hstring> Split(std::string const& source)
		{
			std::vector<hstring> result;

			std::string_view text{ source };

			size_t begin = 0;

			while (begin <= text.size())
			{
				auto end = text.find('|', begin);

				auto part =	text.substr(
					begin,
					end == std::string_view::npos
					? std::string_view::npos
					: end - begin);

				if (!part.empty())
				{
					result.emplace_back(
						winrt::to_hstring(
							std::string{ part }));
				}

				if (end == std::string_view::npos)
					break;

				begin = end + 1;
			}

			return result;
		}
	}


	// =========================================================
	// Tab order
	// =========================================================

	inline void SaveTabViewOrder(TabView const& tabView, std::string const& key)
	{
		if (!tabView)
			return;

		auto items = tabView.TabItems();

		std::string result;

		for (uint32_t i = 0; i < items.Size(); ++i)
		{
			auto item = items.GetAt(i).try_as<TabViewItem>();

			if (!item)
				continue;

			auto id = details::GetItemId(item);

			if (id.empty())
				continue;

			if (!result.empty())
				result += '|';

			result += winrt::to_string(id);
		}

		::OpenNet::Core::AppSettingsDatabase::Instance().SetString(
			std::string{ StateCategory },
			details::MakeOrderKey(key),
			result);
	}


	inline bool RestoreTabViewOrder(TabView const& tabView, std::string const& key)
	{
		if (!tabView)
			return false;

		auto saved = ::OpenNet::Core::AppSettingsDatabase::Instance().GetString(
			std::string{ StateCategory },
			details::MakeOrderKey(key));

		if (!saved || saved->empty())
			return false;

		auto savedOrder = details::Split(*saved);

		if (savedOrder.empty())
			return false;

		auto items = tabView.TabItems();

		struct Entry
		{
			hstring Id;
			TabViewItem Item;
			bool Restored{};
		};

		std::vector<Entry> entries;

		entries.reserve(items.Size());

		/*
		 * Snapshot all TabViewItems first.
		 *
		 * These WinRT objects hold strong COM references,
		 * so items.Clear() won't destroy them while entries
		 * still owns references.
		 */
		for (uint32_t i = 0; i < items.Size(); ++i)
		{
			auto item = items.GetAt(i).try_as<TabViewItem>();

			if (!item)
				continue;

			auto id = details::GetItemId(item);

			if (id.empty())
				continue;

			entries.push_back(
				Entry
				{
					.Id = std::move(id),
					.Item = std::move(item),
					.Restored = false
				});
		}

		if (entries.empty())
			return false;

		items.Clear();

		/*
		 * Restore known tabs according to persisted order.
		 */
		for (auto const& savedId : savedOrder)
		{
			for (auto& entry : entries)
			{
				if (entry.Restored)
					continue;

				if (entry.Id == savedId)
				{
					items.Append(entry.Item);
					entry.Restored = true;
					break;
				}
			}
		}

		/*
		 * Tabs introduced by a newer version are appended
		 * according to their original XAML order.
		 */
		for (auto& entry : entries)
		{
			if (entry.Restored)
				continue;

			items.Append(entry.Item);

			entry.Restored = true;
		}

		return true;
	}


	// =========================================================
	// Selected tab
	// =========================================================

	inline void SaveSelectedTab(TabView const& tabView, std::string const& key)
	{
		if (!tabView)
			return;

		auto selectedItem = tabView.SelectedItem().try_as<TabViewItem>();

		if (!selectedItem)
			return;

		auto id = details::GetItemId(
			selectedItem);

		if (id.empty())
			return;

		::OpenNet::Core::AppSettingsDatabase::Instance().SetString(
			std::string{ StateCategory },
			details::MakeSelectedKey(key),
			winrt::to_string(id));
	}


	inline bool RestoreSelectedTab(TabView const& tabView, std::string const& key)
	{
		if (!tabView)
			return false;

		auto saved = ::OpenNet::Core::AppSettingsDatabase::Instance().GetString(
			std::string{ StateCategory },
			details::MakeSelectedKey(key));

		if (!saved || saved->empty())
		{
			return false;
		}

		auto selectedId = winrt::to_hstring(*saved);
		auto items = tabView.TabItems();

		for (uint32_t i = 0; i < items.Size(); ++i)
		{
			auto item = items.GetAt(i).try_as<TabViewItem>();

			if (!item)
				continue;

			if (details::GetItemId(item) == selectedId)
			{
				tabView.SelectedItem(item);

				return true;
			}
		}

		return false;
	}

	inline void SaveTabWidthMode(TabViewWidthMode value, std::string const& key)
	{
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(
			std::string{ StateCategory },
			details::MakeSettingKey(key, "TabWidthMode"),
			static_cast<int64_t>(value));
	}

	inline std::optional<TabViewWidthMode> GetTabWidthMode(std::string const& key)
	{
		auto saved =
			::OpenNet::Core::AppSettingsDatabase::Instance().GetInt(
				std::string{ StateCategory },
				details::MakeSettingKey(key, "TabWidthMode"));

		if (!saved)
			return std::nullopt;

		switch (*saved)
		{
			case static_cast<int64_t>(TabViewWidthMode::Equal):
				return TabViewWidthMode::Equal;

			case static_cast<int64_t>(TabViewWidthMode::SizeToContent):
				return TabViewWidthMode::SizeToContent;

			case static_cast<int64_t>(TabViewWidthMode::Compact):
				return TabViewWidthMode::Compact;

			default:
				return std::nullopt;
		}
	}

	// =========================================================
	// Complete state
	// =========================================================

	inline void SaveTabViewState(TabView const& tabView, std::string const& key)
	{
		SaveTabViewOrder(tabView, key);
		SaveSelectedTab(tabView, key);
	}


	inline void RestoreTabViewState(TabView const& tabView, std::string const& key)
	{
		if (!tabView)
			return;

		/*
		 * Restore collection structure first.
		 */
		RestoreTabViewOrder(tabView, key);

		/*
		 * Restore selection after collection order.
		 */
		if (!RestoreSelectedTab(tabView, key))
		{
			if (tabView.TabItems().Size() != 0)
			{
				tabView.SelectedIndex(0);
			}
		}
	}
}