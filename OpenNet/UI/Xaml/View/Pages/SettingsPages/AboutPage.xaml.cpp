#include <windows.h>
#include "XamlWorkaround.h"
#include "Web/Request/Builder/GithubRequest.h"
#include "AboutPage.xaml.h"
#include "SettingsPageTagRegister.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/AboutPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/AboutPage.g.cpp"
#endif
#if defined GetObject
#undef GetObject
#endif

import OpenNet.Core.ApplicationModel;
import winrt.Windows.System;
import winrt.Windows.Data.Json;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace ::mvvm;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	static SettingsPageTagRegister<AboutPage> s_tags{
		L"about", L"SettingsAboutSearchTags" };
	AboutPage::AboutPage()
	{
		loadRepoInfos();
		loadCommitMessage();

		Loaded([this](auto&&, auto&&)
		{
			LoadAppVersion();
		});
	}

	winrt::fire_and_forget AboutPage::LoadAppVersion()
	{
		co_await winrt::resume_background();
		auto v = winrt::to_hstring(::OpenNet::Core::ApplicationModel::PackageIdentityAdapter::GetAppVersion().ToString().c_str());
		co_await winrtplus::resume_foreground(VersionText().DispatcherQueue());
		VersionText().Text(v);
	}


	winrt::hstring AboutPage::WASDKReleaseVersion()
	{
		return ::OpenNet::Core::ApplicationModel::PackageIdentityAdapter::GetWindowsAppSdkReleaseVersion();
	}

	winrt::hstring AboutPage::WASDKRuntimeVersion()
	{
		return ::OpenNet::Core::ApplicationModel::PackageIdentityAdapter::GetWindowsAppSdkRuntimeVersion();
	}

	winrt::hstring AboutPage::FormatVersion(
		uint32_t major,
		uint32_t minor,
		uint32_t patch,
		uint32_t majorMinor,
		winrt::hstring const& channel)
	{
		return winrt::hstring{ std::format(
			L"{}.{}.{}.{}-{}",
			major,
			minor,
			patch,
			majorMinor,
			channel
		) };
	}

	int AboutPage::Stars()
	{
		return m_repoInfo ? m_repoInfo->Stars : 0;
	}

	int AboutPage::Forks()
	{
		return m_repoInfo ? m_repoInfo->Forks : 0;
	}

	int AboutPage::Issues()
	{
		return m_repoInfo ? m_repoInfo->Issues : 0;
	}

	winrt::hstring AboutPage::UpdatedAt()
	{
		return m_repoInfo ? m_repoInfo->UpdatedAt : L"";
	}

	winrt::hstring AboutPage::CommitMessage()
	{
		return m_commitMessage;
	}

	void AboutPage::SettingsCard_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri{ L"https://github.com/hoshiizumiya/OpenNet" });
	}

	bool AboutPage::IsLoadingRepoInfo()
	{
		return !m_repoInfo.has_value();
	}

	winrt::fire_and_forget AboutPage::loadRepoInfos()
	{
		try
		{
			auto result = co_await client.SendRequestAsync(GithubRequest{ L"https://api.github.com/repos/hoshiizumiya/OpenNet" });
			auto resultStr = co_await result.Content().ReadAsStringAsync();

			m_repoInfo.emplace(winrt::Windows::Data::Json::JsonObject::Parse(resultStr));
			// Notify bindings that repo info has been loaded and properties changed
			RaisePropertyChangedEvent(L"IsLoadingRepoInfo");
			RaisePropertyChangedEvent(L"Stars");
			RaisePropertyChangedEvent(L"Forks");
			RaisePropertyChangedEvent(L"Issues");
			RaisePropertyChangedEvent(L"UpdatedAt");
		}
		catch (...)
		{
		}
	}

	winrt::fire_and_forget AboutPage::loadCommitMessage()
	{
		try
		{
			auto result = co_await client.SendRequestAsync(GithubRequest{ L"https://api.github.com/repos/hoshiizumiya/OpenNet/commits?per_page=1" });
			auto resultStr = co_await result.Content().ReadAsStringAsync();
			m_commitMessage = winrt::Windows::Data::Json::JsonArray::Parse(resultStr).GetAt(0).GetObject().GetNamedObject(L"commit").GetNamedString(L"message");
			RaisePropertyChangedEvent(L"CommitMessage");
		}
		catch (...)
		{
		}
	}
}
