#include "XamlWorkaround.h"
#include "InfoBarTemplateSelector.h"
#if __has_include("UI/Xaml/Control/InfoBarTemplateSelector.g.cpp")
#include "UI/Xaml/Control/InfoBarTemplateSelector.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	DataTemplate InfoBarTemplateSelector::ActionButtonEnabled() const
	{
		return m_actionButtonEnabled;
	}

	void InfoBarTemplateSelector::ActionButtonEnabled(
		DataTemplate const& value)
	{
		m_actionButtonEnabled = value;
	}

	DataTemplate InfoBarTemplateSelector::ActionButtonDisabled() const
	{
		return m_actionButtonDisabled;
	}

	void InfoBarTemplateSelector::ActionButtonDisabled(
		DataTemplate const& value)
	{
		m_actionButtonDisabled = value;
	}

	DataTemplate InfoBarTemplateSelector::SelectTemplateCore(
		Windows::Foundation::IInspectable const& item)
	{
		if (auto options =
			item.try_as<OpenNet::Service::Notification::InfoBarOptions>())
		{
			if (options.ActionButtonContent() &&
				options.ActionButtonCommand())
			{
				return m_actionButtonEnabled;
			}
		}
		return m_actionButtonDisabled;
	}

	DataTemplate InfoBarTemplateSelector::SelectTemplateCore(
		Windows::Foundation::IInspectable const& item,
		DependencyObject const&)
	{
		return SelectTemplateCore(item);
	}
}
