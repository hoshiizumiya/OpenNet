#include <Shlwapi.h>

#include "XamlWorkaround.h"
#include "ThemesSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp"
#endif

#include "FontCustomizePage.xaml.h"
#include <include/ScopedButtonDisabler.hpp>
#include "MainSettingsPage.xaml.h"
#include "ThemeSettingBackdropCustomizePage.xaml.h"
#include "Service/Background/BackgroundMediaService.h"
#include "SettingsPageTagRegister.h"
#include "UI/Xaml/Control/Effect/AnimatedDigit.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.MaterialTheme;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.WinUI.LiquidGlass;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;

namespace
{
	constexpr auto kBackdropUseFallbackKey         = "backdrop_use_fallback";
	constexpr auto kAnimatedDigitsKey              = "animated_digits_enabled";

	std::optional<bool> MaterialSwitchValue(winrt::Windows::Foundation::IInspectable const& sender)
	{
		if (!sender) return std::nullopt;
		if (auto standard = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch>())
			return standard.IsOn();
		if (auto glass = sender.try_as<winrt::WinUI::LiquidGlass::LiquidGlassToggleSwitch>())
			return glass.IsOn();
		return std::nullopt;
	}

	void SetMaterialSwitchValue(winrt::Windows::Foundation::IInspectable const& target, bool value)
	{
		if (!target) return;
		if (auto standard = target.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch>())
			standard.IsOn(value);
		if (auto glass = target.try_as<winrt::WinUI::LiquidGlass::LiquidGlassToggleSwitch>())
			glass.IsOn(value);
	}
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

	void ThemesSettingsPage::MaterialStyleSelector_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		auto const index = MaterialStyleSelector().SelectedIndex();
		if (index >= 0 && index <= 1)
		{
			::OpenNet::Helpers::MaterialTheme::Set(static_cast<::OpenNet::Helpers::MaterialStyle>(index));
		}
	}

	void ThemesSettingsPage::LoadBackdropSettings()
	{
		m_isInitializing = true;
		MaterialStyleSelector().SelectedIndex(static_cast<int>(::OpenNet::Helpers::MaterialTheme::Current()));

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
		if (auto standard = ImageOpacitySlider()) standard.Value(mediaOptions.ImageOpacity);
		if (auto glass = ImageOpacityGlassSlider()) glass.Value(mediaOptions.ImageOpacity);
		SetMaterialSwitchValue(BackdropFallbackSwitch(), useFallback);
		SetMaterialSwitchValue(BackdropFallbackGlassSwitch(), useFallback);
		SetMaterialSwitchValue(ApplyBackgroundToSecondaryWindowsSwitch(), applyBackgroundToSecondaryWindows);
		SetMaterialSwitchValue(ApplyBackgroundToSecondaryWindowsGlassSwitch(), applyBackgroundToSecondaryWindows);
		const bool animatedDigits = db.GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			kAnimatedDigitsKey).value_or(false);
		if (auto standard = AnimatedDigitsSwitch()) standard.IsOn(animatedDigits);
		if (auto glass = AnimatedDigitsGlassSwitch()) glass.IsOn(animatedDigits);
		ImageModeComboBox().SelectedIndex(static_cast<int>(mediaOptions.ImageMode));
		VideoModeComboBox().SelectedIndex(static_cast<int>(mediaOptions.VideoMode));
		VideoStretchComboBox().SelectedIndex(mediaOptions.VideoStretch);
		if (auto standard = VideoOpacitySlider()) standard.Value(mediaOptions.VideoOpacity);
		if (auto glass = VideoOpacityGlassSlider()) glass.Value(mediaOptions.VideoOpacity);
		SetMaterialSwitchValue(VideoMutedSwitch(), mediaOptions.VideoMuted);
		SetMaterialSwitchValue(VideoMutedGlassSwitch(), mediaOptions.VideoMuted);
		SetMaterialSwitchValue(VideoLoopingSwitch(), mediaOptions.VideoLooping);
		SetMaterialSwitchValue(VideoLoopingGlassSwitch(), mediaOptions.VideoLooping);
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
		if (auto standard = BackdropFallbackSwitch()) standard.IsEnabled(backgroundType == 1);
		if (auto glass = BackdropFallbackGlassSwitch()) glass.IsEnabled(backgroundType == 1);

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
		const bool glassMaterial = ::OpenNet::Helpers::MaterialTheme::Current() == ::OpenNet::Helpers::MaterialStyle::LiquidGlass;
		winrt::Microsoft::UI::Xaml::Controls::Slider imageOpacity = glassMaterial
			? ImageOpacityGlassSlider().try_as<winrt::Microsoft::UI::Xaml::Controls::Slider>()
			: ImageOpacitySlider();
		winrt::Microsoft::UI::Xaml::Controls::Slider videoOpacity = glassMaterial
			? VideoOpacityGlassSlider().try_as<winrt::Microsoft::UI::Xaml::Controls::Slider>()
			: VideoOpacitySlider();
		auto const videoMuted = glassMaterial
			? MaterialSwitchValue(VideoMutedGlassSwitch())
			: MaterialSwitchValue(VideoMutedSwitch());
		auto const videoLooping = glassMaterial
			? MaterialSwitchValue(VideoLoopingGlassSwitch())
			: MaterialSwitchValue(VideoLoopingSwitch());
		auto const rotationMinutes = BackgroundRotationMinutesBox();

		if (!imageMode || !videoMode || !imagePath || !videoPath
			|| !imageStretch || !videoStretch || !imageOpacity || !videoOpacity
			|| !videoMuted.has_value() || !videoLooping.has_value() || !rotationMinutes)
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
		options.VideoMuted = *videoMuted;
		options.VideoLooping = *videoLooping;
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

	void ThemesSettingsPage::BackdropFallbackSwitch_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		auto const enabled = MaterialSwitchValue(sender);
		if (!enabled) return;
		m_isInitializing = true;
		SetMaterialSwitchValue(BackdropFallbackSwitch(), *enabled);
		SetMaterialSwitchValue(BackdropFallbackGlassSwitch(), *enabled);
		m_isInitializing = false;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropUseFallbackKey, *enabled);
		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::BackdropFallbackSwitch_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool enabled = ::OpenNet::Core::AppSettingsDatabase::Instance().GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropUseFallbackKey).value_or(true);
		const bool wasInitializing = m_isInitializing;
		m_isInitializing = true;
		SetMaterialSwitchValue(sender, enabled);
		m_isInitializing = wasInitializing;
	}

	void ThemesSettingsPage::ApplyBackgroundToSecondaryWindowsSwitch_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		auto const enabled = MaterialSwitchValue(sender);
		if (!enabled) return;
		m_isInitializing = true;
		SetMaterialSwitchValue(ApplyBackgroundToSecondaryWindowsSwitch(), *enabled);
		SetMaterialSwitchValue(ApplyBackgroundToSecondaryWindowsGlassSwitch(), *enabled);
		m_isInitializing = false;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			::OpenNet::Helpers::kApplyBackgroundToSecondaryWindowsKey,
			*enabled);
		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::ApplyBackgroundToSecondaryWindowsSwitch_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool enabled = ::OpenNet::Core::AppSettingsDatabase::Instance().GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			::OpenNet::Helpers::kApplyBackgroundToSecondaryWindowsKey).value_or(true);
		const bool wasInitializing = m_isInitializing;
		m_isInitializing = true;
		SetMaterialSwitchValue(sender, enabled);
		m_isInitializing = wasInitializing;
	}

	void ThemesSettingsPage::AnimatedDigitsSwitch_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		auto const enabled = MaterialSwitchValue(sender);
		if (!enabled) return;
		m_isInitializing = true;
		SetMaterialSwitchValue(AnimatedDigitsSwitch(), *enabled);
		SetMaterialSwitchValue(AnimatedDigitsGlassSwitch(), *enabled);
		m_isInitializing = false;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			kAnimatedDigitsKey,
			*enabled);
		winrt::OpenNet::UI::Xaml::Control::Effect::implementation::AnimatedDigit::AnimationsEnabled(*enabled);
	}

	void ThemesSettingsPage::AnimatedDigitsControl_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool enabled = ::OpenNet::Core::AppSettingsDatabase::Instance().GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			kAnimatedDigitsKey).value_or(false);
		const bool wasInitializing = m_isInitializing;
		m_isInitializing = true;
		SetMaterialSwitchValue(sender, enabled);
		m_isInitializing = wasInitializing;
	}

	winrt::Windows::Foundation::IAsyncAction ThemesSettingsPage::SetImageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		ScopedButtonDisabler disabler{ sender };
		auto lifetime = get_strong();
		using namespace ::OpenNet::Service::Background;
		auto const mode = static_cast<BackgroundSourceMode>(ImageModeComboBox().SelectedIndex());
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

	void ThemesSettingsPage::BackgroundRotationMinutesBox_ValueChanged(NumberBox const& sender, NumberBoxValueChangedEventArgs const&)
	{
		if (m_isInitializing || std::isnan(sender.Value())) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	winrt::Windows::Foundation::IAsyncAction ThemesSettingsPage::SetVideoButton_Click(IInspectable const& sender, RoutedEventArgs const&)
	{
		ScopedButtonDisabler disabler{ sender };
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

	void ThemesSettingsPage::ClearVideoButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		VideoPathText().Text(L"");
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoModeComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		UpdateMediaCardState();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoMutedSwitch_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		auto const enabled = MaterialSwitchValue(sender);
		if (!enabled) return;
		m_isInitializing = true;
		SetMaterialSwitchValue(VideoMutedSwitch(), *enabled);
		SetMaterialSwitchValue(VideoMutedGlassSwitch(), *enabled);
		m_isInitializing = false;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoMutedSwitch_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool wasInitializing = m_isInitializing;
		m_isInitializing = true;
		SetMaterialSwitchValue(sender, ::OpenNet::Service::Background::GetBackgroundMediaService().LoadOptions().VideoMuted);
		m_isInitializing = wasInitializing;
	}

	void ThemesSettingsPage::VideoLoopingSwitch_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_isInitializing) return;
		auto const enabled = MaterialSwitchValue(sender);
		if (!enabled) return;
		m_isInitializing = true;
		SetMaterialSwitchValue(VideoLoopingSwitch(), *enabled);
		SetMaterialSwitchValue(VideoLoopingGlassSwitch(), *enabled);
		m_isInitializing = false;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoLoopingSwitch_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool wasInitializing = m_isInitializing;
		m_isInitializing = true;
		SetMaterialSwitchValue(sender, ::OpenNet::Service::Background::GetBackgroundMediaService().LoadOptions().VideoLooping);
		m_isInitializing = wasInitializing;
	}

	void ThemesSettingsPage::VideoStretchComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoOpacitySlider_ValueChanged(IInspectable const& sender, RangeBaseValueChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		if (auto slider = sender.try_as<Slider>())
		{
			m_isInitializing = true;
			if (auto standard = VideoOpacitySlider()) standard.Value(slider.Value());
			if (auto glass = VideoOpacityGlassSlider()) glass.Value(slider.Value());
			m_isInitializing = false;
		}
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::VideoOpacitySlider_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (auto slider = sender.try_as<Slider>())
		{
			const bool wasInitializing = m_isInitializing;
			m_isInitializing = true;
			slider.Value(::OpenNet::Service::Background::GetBackgroundMediaService().LoadOptions().VideoOpacity);
			m_isInitializing = wasInitializing;
		}
	}

	void ThemesSettingsPage::ImageOpacitySlider_ValueChanged(IInspectable const& sender, RangeBaseValueChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		if (auto slider = sender.try_as<Slider>())
		{
			m_isInitializing = true;
			if (auto standard = ImageOpacitySlider()) standard.Value(slider.Value());
			if (auto glass = ImageOpacityGlassSlider()) glass.Value(slider.Value());
			m_isInitializing = false;
		}
		PersistMediaOptions();
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ImageOpacitySlider_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (auto slider = sender.try_as<Slider>())
		{
			const bool wasInitializing = m_isInitializing;
			m_isInitializing = true;
			slider.Value(::OpenNet::Service::Background::GetBackgroundMediaService().LoadOptions().ImageOpacity);
			m_isInitializing = wasInitializing;
		}
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
