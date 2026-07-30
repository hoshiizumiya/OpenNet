#pragma once

#include "UI/Xaml/Control/CheckUpdateControl.g.h"

import OpenNet.Helpers.EnsureDependencyProperties;
import OpenNet.Helpers.TemplateControlHelper;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Windows.Foundation;

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	struct CheckUpdateControl :
		CheckUpdateControlT<CheckUpdateControl>,
		TemplateControlHelper<CheckUpdateControl>,
		EnsureDependencyProperty<CheckUpdateControl>
	{
		using CheckUpdateControlT<CheckUpdateControl>::DefaultStyleKey;

		CheckUpdateControl() = default;
		~CheckUpdateControl();

		static void EnsureDependencyProperties();
		void OnApplyTemplate();

		static winrt::Microsoft::UI::Xaml::DependencyProperty IsUpdateAvailableProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty UpdateAvailableTitleProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty UpdateAvailableVersionTitleProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty UpdateAvailableVersionProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty UpdateAvailableIconProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty UpdateNotAvailableTitleProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty LastUpdateCheckTitleProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty LastUpdateCheckDateProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty UpdateNotAvailableIconProperty();

		bool IsUpdateAvailable();
		void IsUpdateAvailable(bool value);
		winrt::Windows::Foundation::IInspectable UpdateAvailableTitle();
		void UpdateAvailableTitle(winrt::Windows::Foundation::IInspectable const& value);
		winrt::hstring UpdateAvailableVersionTitle();
		void UpdateAvailableVersionTitle(winrt::hstring const& value);
		winrt::hstring UpdateAvailableVersion();
		void UpdateAvailableVersion(winrt::hstring const& value);
		winrt::Windows::Foundation::IInspectable UpdateAvailableIcon();
		void UpdateAvailableIcon(winrt::Windows::Foundation::IInspectable const& value);
		winrt::Windows::Foundation::IInspectable UpdateNotAvailableTitle();
		void UpdateNotAvailableTitle(winrt::Windows::Foundation::IInspectable const& value);
		winrt::hstring LastUpdateCheckTitle();
		void LastUpdateCheckTitle(winrt::hstring const& value);
		winrt::hstring LastUpdateCheckDate();
		void LastUpdateCheckDate(winrt::hstring const& value);
		winrt::Windows::Foundation::IInspectable UpdateNotAvailableIcon();
		void UpdateNotAvailableIcon(winrt::Windows::Foundation::IInspectable const& value);

		winrt::event_token Click(
			winrt::Windows::Foundation::EventHandler<
				winrt::Microsoft::UI::Xaml::RoutedEventArgs> const& handler);
		void Click(winrt::event_token const& token) noexcept;

		static constexpr wchar_t const* ResourceUri =
			L"ms-appx:///UI/Xaml/Control/CheckUpdateControl_ResourceDictionary.xaml";

	private:
		static void OnUpdateAvailableTitleChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		static void OnUpdateAvailableIconChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		static void OnUpdateNotAvailableTitleChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		static void OnUpdateNotAvailableIconChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);

		void UpdateAvailableTitlePresentation();
		void UpdateAvailableIconPresentation();
		void UpdateNotAvailableTitlePresentation();
		void UpdateNotAvailableIconPresentation();
		void OnUpdateAvailableButton(
			winrt::Windows::Foundation::IInspectable const&,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void DetachButtonHandler() noexcept;
		static bool IsStringContent(winrt::Windows::Foundation::IInspectable const& value);

		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_isUpdateAvailableProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_updateAvailableTitleProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_updateAvailableVersionTitleProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_updateAvailableVersionProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_updateAvailableIconProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_updateNotAvailableTitleProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_lastUpdateCheckTitleProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_lastUpdateCheckDateProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty s_updateNotAvailableIconProperty{ nullptr };

		winrt::Microsoft::UI::Xaml::Controls::ContentPresenter m_updateAvailableIconPresenter{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::Border m_updateAvailableIconBorder{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ContentPresenter m_updateNotAvailableIconPresenter{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::Border m_updateNotAvailableIconBorder{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ContentPresenter m_updateAvailableTitlePresenter{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::StackPanel m_updateAvailableTitleStackPanel{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ContentPresenter m_updateNotAvailableTitlePresenter{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::StackPanel m_updateNotAvailableTitleStackPanel{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::Button m_updateAvailableButton{ nullptr };
		winrt::event_token m_updateAvailableButtonClickToken{};
		winrt::event<winrt::Windows::Foundation::EventHandler<
			winrt::Microsoft::UI::Xaml::RoutedEventArgs>> m_click;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::factory_implementation
{
	struct CheckUpdateControl : CheckUpdateControlT<CheckUpdateControl, implementation::CheckUpdateControl>
	{
	};
}
