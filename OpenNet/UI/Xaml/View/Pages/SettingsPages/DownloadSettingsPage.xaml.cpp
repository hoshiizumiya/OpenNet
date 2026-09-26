#include "XamlWorkaround.h"
#include "DownloadSettingsPage.xaml.h"
#include "SettingsPageTagRegister.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/DownloadSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/DownloadSettingsPage.g.cpp"
#endif

#include <include/ScopedButtonDisabler.hpp>

import OpenNet.Core.TorrentSettings;
import OpenNet.Core.AppSettingsDatabase;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.WinUI.LiquidGlass;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.Windows.Storage.Pickers;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::Windows::Storage::Pickers;
using namespace winrt::Windows::Foundation;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	static SettingsPageTagRegister<DownloadSettingsPage> s_tags{ L"download", L"SettingsDownloadSearchTags" };
	namespace
	{
		std::optional<bool> ToggleIsOn(IInspectable const& sender)
		{
			if (!sender) return std::nullopt;
			if (auto standard = sender.try_as<ToggleSwitch>()) return standard.IsOn();
			if (auto glass = sender.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) return glass.IsOn();
			return std::nullopt;
		}
		void SetToggleIsOn(IInspectable const& target, bool value)
		{
			if (!target) return;
			if (auto standard = target.try_as<ToggleSwitch>()) standard.IsOn(value);
			if (auto glass = target.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) glass.IsOn(value);
		}
	}
	DownloadSettingsPage::DownloadSettingsPage()
	{
		InitializeComponent();

		Loaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			LoadSettings();
		});

		// Save settings when page unloads so TextBox LostFocus changes aren't lost
		Unloaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			if (!m_loading)
			{
				SaveSettings();
			}
		});
	}

	void DownloadSettingsPage::LoadSettings()
	{
		m_loading = true;
		auto& mgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
		mgr.Load();
		PopulateFromSettings(mgr.Get());
		m_loading = false;
	}

	void DownloadSettingsPage::PopulateFromSettings(::OpenNet::Core::TorrentSettings const& s)
	{
		DefaultSavePathTextBox().Text(s.defaultSavePath);
		m_preallocateStorage = s.preallocateStorage;
		m_autoStartDownloads = s.autoStartDownloads;
		m_recheckBeforeResume = s.recheckBeforeResume;
		m_moveCompleted = s.moveCompletedEnabled;
		SetToggleIsOn(PreallocateStorageToggle(), m_preallocateStorage);
		SetToggleIsOn(PreallocateStorageGlassToggle(), m_preallocateStorage);
		SetToggleIsOn(AutoStartDownloadsToggle(), m_autoStartDownloads);
		SetToggleIsOn(AutoStartDownloadsGlassToggle(), m_autoStartDownloads);
		SetToggleIsOn(RecheckBeforeResumeToggle(), m_recheckBeforeResume);
		SetToggleIsOn(RecheckBeforeResumeGlassToggle(), m_recheckBeforeResume);
		SetToggleIsOn(MoveCompletedToggle(), m_moveCompleted);
		SetToggleIsOn(MoveCompletedGlassToggle(), m_moveCompleted);
		MoveCompletedPathTextBox().Text(s.moveCompletedPath);
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		Aria2ConnectionsNumberBox().Value(static_cast<double>(database.GetInt(
			::OpenNet::Core::AppSettingsDatabase::CAT_DOWNLOAD,
			"aria2_connections_per_server", 8)));
		m_httpP2PPreferred = database.GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_DOWNLOAD,
			"http_p2p_preferred",
			true).value_or(true);
		SetToggleIsOn(
			HttpP2PPreferredToggle(),
			m_httpP2PPreferred);
		SetToggleIsOn(
			HttpP2PPreferredGlassToggle(),
			m_httpP2PPreferred);
	}

	void DownloadSettingsPage::OnSettingChanged(IInspectable const& sender, IInspectable const&)
	{
		if (m_loading)
			return;
		if (auto value = ToggleIsOn(sender))
		{
			auto const name = sender.as<FrameworkElement>().Name();
			if (name == L"PreallocateStorageToggle" || name == L"PreallocateStorageGlassToggle") m_preallocateStorage = *value;
			else if (name == L"AutoStartDownloadsToggle" || name == L"AutoStartDownloadsGlassToggle") m_autoStartDownloads = *value;
			else if (name == L"RecheckBeforeResumeToggle" || name == L"RecheckBeforeResumeGlassToggle") m_recheckBeforeResume = *value;
			else if (name == L"MoveCompletedToggle" || name == L"MoveCompletedGlassToggle") m_moveCompleted = *value;
			else if (name == L"HttpP2PPreferredToggle" || name == L"HttpP2PPreferredGlassToggle") m_httpP2PPreferred = *value;
		}
		SaveSettings();
	}

	void DownloadSettingsPage::MaterialToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		auto const name = sender.as<FrameworkElement>().Name();
		const bool wasLoading = m_loading;
		m_loading = true;
		if (name == L"PreallocateStorageToggle" || name == L"PreallocateStorageGlassToggle") SetToggleIsOn(sender, m_preallocateStorage);
		else if (name == L"AutoStartDownloadsToggle" || name == L"AutoStartDownloadsGlassToggle") SetToggleIsOn(sender, m_autoStartDownloads);
		else if (name == L"RecheckBeforeResumeToggle" || name == L"RecheckBeforeResumeGlassToggle") SetToggleIsOn(sender, m_recheckBeforeResume);
		else if (name == L"MoveCompletedToggle" || name == L"MoveCompletedGlassToggle") SetToggleIsOn(sender, m_moveCompleted);
		else if (name == L"HttpP2PPreferredToggle" || name == L"HttpP2PPreferredGlassToggle") SetToggleIsOn(sender, m_httpP2PPreferred);
		m_loading = wasLoading;
	}

	void DownloadSettingsPage::SaveSettings()
	{
		auto& mgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
		auto s = mgr.Get();

		s.defaultSavePath = DefaultSavePathTextBox().Text();
		s.preallocateStorage = m_preallocateStorage;
		s.autoStartDownloads = m_autoStartDownloads;
		s.recheckBeforeResume = m_recheckBeforeResume;
		s.moveCompletedEnabled = m_moveCompleted;
		s.moveCompletedPath = MoveCompletedPathTextBox().Text();
		mgr.Set(s);
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		database.SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_DOWNLOAD,
						"aria2_connections_per_server",
						static_cast<std::int64_t>(std::clamp(
							Aria2ConnectionsNumberBox().Value(), 1.0, 16.0)));
		database.SetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_DOWNLOAD,
			"http_p2p_preferred",
			m_httpP2PPreferred);
	}

	winrt::fire_and_forget DownloadSettingsPage::BrowseSavePathButton_Click(IInspectable const& sender, RoutedEventArgs const&)
	{
		ScopedButtonDisabler disabler{ sender };
		co_await PickFolder(DefaultSavePathTextBox());
	}

	winrt::fire_and_forget DownloadSettingsPage::BrowseMovePathButton_Click(IInspectable const& sender, RoutedEventArgs const&)
	{
		ScopedButtonDisabler disabler{ sender };
		co_await PickFolder(MoveCompletedPathTextBox());
	}

	IAsyncAction DownloadSettingsPage::PickFolder(TextBox target)
	{
		auto folderPicker = FolderPicker(XamlRoot().ContentIslandEnvironment().AppWindowId());
		folderPicker.SuggestedStartLocation(PickerLocationId::Downloads);

		auto folder = co_await folderPicker.PickSingleFolderAsync();
		if (folder)
		{
			target.Text(folder.Path());
			SaveSettings();
		}
	}
}
