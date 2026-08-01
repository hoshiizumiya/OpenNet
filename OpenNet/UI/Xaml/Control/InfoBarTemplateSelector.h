#pragma once

#include "Service/Notification/InfoBarOptions.h"
#include "UI/Xaml/Control/InfoBarTemplateSelector.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	struct InfoBarTemplateSelector : InfoBarTemplateSelectorT<InfoBarTemplateSelector>
	{
		InfoBarTemplateSelector() = default;

		winrt::Microsoft::UI::Xaml::DataTemplate ActionButtonEnabled() const;
		void ActionButtonEnabled(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
		winrt::Microsoft::UI::Xaml::DataTemplate ActionButtonDisabled() const;
		void ActionButtonDisabled(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
		winrt::Microsoft::UI::Xaml::DataTemplate SelectTemplateCore(winrt::Windows::Foundation::IInspectable const& item);
		winrt::Microsoft::UI::Xaml::DataTemplate SelectTemplateCore(winrt::Windows::Foundation::IInspectable const& item, winrt::Microsoft::UI::Xaml::DependencyObject const& container);

	private:
		winrt::Microsoft::UI::Xaml::DataTemplate m_actionButtonEnabled{ nullptr };
		winrt::Microsoft::UI::Xaml::DataTemplate m_actionButtonDisabled{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::factory_implementation
{
	struct InfoBarTemplateSelector : InfoBarTemplateSelectorT<InfoBarTemplateSelector, implementation::InfoBarTemplateSelector>
	{
	};
}
