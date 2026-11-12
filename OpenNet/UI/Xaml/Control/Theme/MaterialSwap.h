#pragma once
#include "UI/Xaml/Control/Theme/MaterialSwap.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
	// Only the slider and the password/search wrappers require separate instances.
	struct MaterialSwap : MaterialSwapT<MaterialSwap>
	{
		MaterialSwap();
		Windows::Foundation::IInspectable Native() const;
		void Native(Windows::Foundation::IInspectable const& value);
		static Microsoft::UI::Xaml::DependencyProperty NativeProperty();
		Windows::Foundation::IInspectable Glass() const;
		void Glass(Windows::Foundation::IInspectable const& value);
		static Microsoft::UI::Xaml::DependencyProperty GlassProperty();
		void OnApplyTemplate();
	private:
		static void Changed(Microsoft::UI::Xaml::DependencyObject const&, Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		void UpdateContent();
		void LoadedHandler(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void UnloadedHandler(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void Detach();
		std::vector<std::pair<Microsoft::UI::Xaml::DependencyProperty, std::int64_t>> m_tokens;
		winrt::OpenNet::UI::Xaml::Control::Theme::MaterialControlState m_state{ nullptr };
	};
}
namespace winrt::OpenNet::UI::Xaml::Control::Theme::factory_implementation
{
	struct MaterialSwap : MaterialSwapT<MaterialSwap, implementation::MaterialSwap>
	{
	};
}
