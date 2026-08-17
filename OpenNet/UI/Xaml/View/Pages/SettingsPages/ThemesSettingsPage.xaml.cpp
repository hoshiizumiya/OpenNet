#include <Shlwapi.h>

#include "XamlWorkaround.h"
#include "ThemesSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp"
#endif

#include "MainSettingsPage.xaml.h"
#include "ThemeSettingBackdropCustomizePage.xaml.h"
#include "FontCustomizePage.xaml.h"
#include "Service/Background/BackgroundMediaService.h"
#include "UI/Xaml/Control/Effect/AnimatedDigit.h"
#include "SettingsPageTagRegister.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml.Controls;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;

namespace
{
	constexpr auto kBackdropUseFallbackKey         = "backdrop_use_fallback";
	constexpr auto kAnimatedDigitsKey              = "animated_digits_enabled";
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	static SettingsPageTagRegister<ThemesSettingsPage> s_tags{
		L"appearance", L"SettingsAppearanceSearchTags" };
	ThemesSettingsPage::ThemesSettingsPage()
	{
		Loaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			LoadBackdropSettings();
		});
	}

	void ThemesSettingsPage::LoadBackdropSettings()
	{
		m_isInitializing = true;

		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto const backgroundType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_type", 1)), 0, 4);
		auto const micaType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "mica_type", 1)), 0, 1);
		auto const acrylicType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "acrylic_type", 0)), 0, 2);
		auto const useFallback = db.GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropUseFallbackKey).value_or(true);
		auto const applyBackgroundToSecondaryWindows = db.GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			::OpenNet::Helpers::kApplyBackgroundToSecondaryWindowsKey)
			.value_or(true);
		auto const mediaOptions = ::OpenNet::Service::Background::
			GetBackgroundMediaService().LoadOptions();

		BackgroundComboBox().SelectedIndex(backgroundType);
		MicaTypeComboBox().SelectedIndex(micaType);
		AcrylicTypeComboBox().SelectedIndex(acrylicType);
		ImageStretchComboBox().SelectedIndex(mediaOptions.ImageStretch);
		ImageOpacitySlider().Value(mediaOptions.ImageOpacity);
		BackdropFallbackSwitch().IsOn(useFallback);
		ApplyBackgroundToSecondaryWindowsSwitch().IsOn(applyBackgroundToSecondaryWindows);
		AnimatedDigitsSwitch().IsOn(db.GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			kAnimatedDigitsKey).value_or(false));
		ImageModeComboBox().SelectedIndex(static_cast<int>(mediaOptions.ImageMode));
		VideoModeComboBox().SelectedIndex(static_cast<int>(mediaOptions.VideoMode));
		VideoStretchComboBox().SelectedIndex(mediaOptions.VideoStretch);
		VideoOpacitySlider().Value(mediaOptions.VideoOpacity);
		VideoMutedSwitch().IsOn(mediaOptions.VideoMuted);
		VideoLoopingSwitch().IsOn(mediaOptions.VideoLooping);
		BackgroundRotationMinutesBox().Value(
			static_cast<double>(mediaOptions.RotationMinutes));
		ImagePathText().Text(mediaOptions.ImagePath);
		VideoPathText().Text(mediaOptions.VideoPath);

		UpdateBackdropCardState();
		UpdateMediaCardState();
		m_isInitializing = false;
	}

	void ThemesSettingsPage::UpdateBackdropCardState()
	{
		auto const backgroundType = static_cast<int>(BackgroundComboBox().SelectedIndex());

		// The type selectors only describe options exposed by the native backdrop.
		// Custom backdrops are configured on the Colors Style page instead.
		MicaTypeCard().IsEnabled(backgroundType == 1);
		AcrylicTypeCard().IsEnabled(backgroundType == 3);

		// BackdropFallbackSwitch enabled only for native Mica (index 1)
		BackdropFallbackSwitch().IsEnabled(backgroundType == 1);

		// SoftBackground (Colors Style) enabled only for custom modes (index 2 or 4)
		bool const isCustomMode = (backgroundType == 2 || backgroundType == 4);
		SoftBackground().IsEnabled(isCustomMode);
	}

	void ThemesSettingsPage::UpdateMediaCardState()
	{
		auto const imageEnabled = ImageModeComboBox().SelectedIndex() > 0;
		auto const videoEnabled = VideoModeComboBox().SelectedIndex() > 0;
		ImageSourceCard().IsEnabled(imageEnabled);
		ClearImageCard().IsEnabled(imageEnabled && !ImagePathText().Text().empty());
		ImageStretchCard().IsEnabled(imageEnabled);
		ImageOpacityCard().IsEnabled(imageEnabled);
		VideoSourceCard().IsEnabled(videoEnabled);
		ClearVideoCard().IsEnabled(videoEnabled && !VideoPathText().Text().empty());
		VideoMutedCard().IsEnabled(videoEnabled);
		VideoLoopingCard().IsEnabled(videoEnabled);
		VideoStretchCard().IsEnabled(videoEnabled);
		VideoOpacityCard().IsEnabled(videoEnabled);
		BackgroundRotationCard().IsEnabled(
			ImageModeComboBox().SelectedIndex() == 2
			|| VideoModeComboBox().SelectedIndex() == 2);
	}

	void ThemesSettingsPage::PersistMediaOptions()
	{
		using namespace ::OpenNet::Service::Background;

		// ValueChanged and SelectionChanged may be raised by LoadComponent while
		// later x:Name fields are still null. Persisting is only meaningful once the
		// complete media-settings surface is connected.
		auto const imageMode = ImageModeComboBox();
		auto const videoMode = VideoModeComboBox();
		auto const imagePath = ImagePathText();
		auto const videoPath = VideoPathText();
		auto const imageStretch = ImageStretchComboBox();
		auto const videoStretch = VideoStretchComboBox();
		auto const imageOpacity = ImageOpacitySlider();
		auto const videoOpacity = VideoOpacitySlider();
		auto const videoMuted = VideoMutedSwitch();
		auto const videoLooping = VideoLoopingSwitch();
		auto const rotationMinutes = BackgroundRotationMinutesBox();

		if (!imageMode || !videoMode || !imagePath || !videoPath
			|| !imageStretch || !videoStretch || !imageOpacity || !videoOpacity
			|| !videoMuted || !videoLooping || !rotationMinutes)
		{
			return;
		}

		BackgroundMediaOptions options;
		options.ImageMode = static_cast<BackgroundSourceMode>(std::clamp(
			imageMode.SelectedIndex(), 0, 2));
		options.VideoMode = static_cast<BackgroundSourceMode>(std::clamp(
			videoMode.SelectedIndex(), 0, 2));
		options.ImagePath = imagePath.Text();
		options.VideoPath = videoPath.Text();
		options.ImageStretch = std::clamp(imageStretch.SelectedIndex(), 0, 3);
		options.VideoStretch = std::clamp(videoStretch.SelectedIndex(), 0, 3);
		options.ImageOpacity = imageOpacity.Value();
		options.VideoOpacity = videoOpacity.Value();
		options.VideoMuted = videoMuted.IsOn();
		options.VideoLooping = videoLooping.IsOn();
		options.RotationMinutes = std::isnan(rotationMinutes.Value())
			? 5
			: static_cast<std::int64_t>(std::round(
				rotationMinutes.Value()));
		GetBackgroundMediaService().SaveOptions(options);
	}

	void ThemesSettingsPage::ApplyBackdropFromSelection()
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::RefreshWindowAppearances();
	}

	void ThemesSettingsPage::ApplyImageBackgroundFromSettings()
	{
		::OpenNet::Service::Background::GetBackgroundMediaService().
			NotifyOptionsChanged();
	}

	void ThemesSettingsPage::BackgroundComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;

		auto const selectedIndex = static_cast<int>(BackgroundComboBox().SelectedIndex());
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_type", selectedIndex);

		UpdateBackdropCardState();
		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::MicaTypeComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;

		auto const selectedIndex = static_cast<int>(MicaTypeComboBox().SelectedIndex());
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "mica_type", selectedIndex);

		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::AcrylicTypeComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;

		auto const selectedIndex = static_cast<int>(AcrylicTypeComboBox().SelectedIndex());
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "acrylic_type", selectedIndex);

		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::BackdropFallbackSwitch_Toggled(IInspectable const&, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropUseFallbackKey, BackdropFallbackSwitch().IsOn());
		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::ApplyBackgroundToSecondaryWindowsSwitch_Toggled(IInspectable const&, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			::OpenNet::Helpers::kApplyBackgroundToSecondaryWindowsKey,
			ApplyBackgroundToSecondaryWindowsSwitch().IsOn());
		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::AnimatedDigitsSwitch_Toggled(
		IInspectable const&, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		auto const enabled = AnimatedDigitsSwitch().IsOn();
		::OpenNet::Core::AppSettingsDatabase::Instance().SetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			kAnimatedDigitsKey,
			enabled);
		winrt::OpenNet::UI::Xaml::Control::Effect::implementation::
			AnimatedDigit::AnimationsEnabled(enabled);
	}

	winrt::Windows::Foundation::IAsyncAction ThemesSettingsPage::SetImageButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		using namespace ::OpenNet::Service::Background;
		auto const mode = static_cast<BackgroundSourceMode>(
			ImageModeComboBox().SelectedIndex());
		if (mode == BackgroundSourceMode::None) co_return;
		auto path = co_await GetBackgroundMediaService().PickSourceAsync(
			XamlRoot().ContentIslandEnvironment().AppWindowId(),
			BackgroundSourceKind::Image,
			mode);
		if (path.empty()) co_return;
		ImagePathText().Text(path);
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ClearImageButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		ImagePathText().Text(L"");
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ImageModeComboBox_SelectionChanged(
		IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ImageStretchComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::BackgroundRotationMinutesBox_ValueChanged(
		NumberBox const& sender, NumberBoxValueChangedEventArgs const&)
	{
		if (m_isInitializing || std::isnan(sender.Value())) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	winrt::Windows::Foundation::IAsyncAction
		ThemesSettingsPage::SetVideoButton_Click(
			IInspectable const&, RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		using namespace ::OpenNet::Service::Background;
		auto const mode = static_cast<BackgroundSourceMode>(
			VideoModeComboBox().SelectedIndex());
		if (mode == BackgroundSourceMode::None) co_return;
		auto path = co_await GetBackgroundMediaService().PickSourceAsync(
			XamlRoot().ContentIslandEnvironment().AppWindowId(),
			BackgroundSourceKind::Video,
			mode);
		if (path.empty()) co_return;
		VideoPathText().Text(path);
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ClearVideoButton_Click(
		IInspectable const&, RoutedEventArgs const&)
	{
		VideoPathText().Text(L"");
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoModeComboBox_SelectionChanged(
		IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoMutedSwitch_Toggled(
		IInspectable const&, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoLoopingSwitch_Toggled(
		IInspectable const&, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoStretchComboBox_SelectionChanged(
		IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoOpacitySlider_ValueChanged(
		IInspectable const&, RangeBaseValueChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ImageOpacitySlider_ValueChanged(IInspectable const&, RangeBaseValueChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::NavigateToThemeSettingBackdropCustomizePageButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (auto host = MainSettingsPage::Current())
		{
			auto items = host->SettingsBarItems();
			while (items.Size() > 2) items.RemoveAtEnd();
			if (items.Size() == 1) items.Append(L"Appearance");
			if (items.Size() == 2) items.Append(L"Colors Style");
		}

		auto transitionInfo = SlideNavigationTransitionInfo{};
		transitionInfo.Effect(SlideNavigationTransitionEffect::FromRight);
		Frame().Navigate(
			xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::ThemeSettingBackdropCustomizePage>(),
			nullptr,
			transitionInfo);
	}

	void ThemesSettingsPage::NavigateToThemeSettingFontSettingPageButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (auto host = MainSettingsPage::Current())
		{
			auto items = host->SettingsBarItems();
			while (items.Size() > 2) items.RemoveAtEnd();
			if (items.Size() == 1) items.Append(L"Appearance");
			if (items.Size() == 2) items.Append(L"Font Setting");
		}

		auto transitionInfo = SlideNavigationTransitionInfo{};
		transitionInfo.Effect(SlideNavigationTransitionEffect::FromRight);
		Frame().Navigate(
			xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::FontCustomizePage>(),
			nullptr,
			transitionInfo);
	}
}
