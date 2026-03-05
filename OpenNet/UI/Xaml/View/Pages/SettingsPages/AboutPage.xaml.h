#pragma once
#include "UI/Xaml/View/Pages/SettingsPages/AboutPage.g.h"
#include "mvvm_framework/notify_property_changed.h"
#include "ViewModels/MainViewModel.h"
#include <winrt/Windows.Web.Http.h>
#include "ThirdParty/RepoInfo.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct AboutPage : AboutPageT<AboutPage>, mvvm::WrapNotifyPropertyChanged<AboutPage>
	{
		AboutPage();
		static winrt::hstring WASDKReleaseVersion();
		static winrt::hstring WASDKRuntimeVersion();
		static winrt::hstring FormatVersion(
			uint32_t major,
			uint32_t minor,
			uint32_t patch,
			uint32_t majorMinor,
			winrt::hstring const& channel
		);
		bool IsLoadingRepoInfo();

		int Stars();
		int Forks();
		int Issues();
		winrt::hstring UpdatedAt();
		winrt::hstring CommitMessage();

		void SettingsCard_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
	private:
		winrt::fire_and_forget LoadAppVersion();
		std::optional<RepoInfo> m_repoInfo;
		winrt::hstring m_commitMessage;
		winrt::hstring m_winuiNugetPackageVersion;
		winrt::hstring m_uwpNugetPackageVersion;
		int m_winuiNugetPackageDownloads{};
		int m_uwpNugetPackageDownloads{};
		winrt::Windows::Web::Http::HttpClient client;
		bool m_isLoadingContributors = true;

		winrt::fire_and_forget loadRepoInfos();
		winrt::fire_and_forget loadCommitMessage();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage>
	{
	};
}
