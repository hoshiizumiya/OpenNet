#include "XamlWorkaround.h"
#include "InfoBarOptions.h"
#if __has_include("Service/Notification/InfoBarOptions.g.cpp")
#include "Service/Notification/InfoBarOptions.g.cpp"
#endif

namespace winrt::OpenNet::Service::Notification::implementation
{
	Microsoft::UI::Xaml::Controls::InfoBarSeverity InfoBarOptions::Severity() const noexcept
	{
		return m_severity;
	}

	void InfoBarOptions::Severity(Microsoft::UI::Xaml::Controls::InfoBarSeverity value) noexcept
	{
		SetProperty(m_severity, value, L"Severity");
	}

	hstring InfoBarOptions::Title() const
	{
		return m_title;
	}
	void InfoBarOptions::Title(hstring const& value)
	{
		SetProperty(m_title, value, L"Title");
	}
	hstring InfoBarOptions::Message() const
	{
		return m_message;
	}
	void InfoBarOptions::Message(hstring const& value)
	{
		SetProperty(m_message, value, L"Message");
	}

	Windows::Foundation::IInspectable InfoBarOptions::Content() const
	{
		return m_content;
	}

	void InfoBarOptions::Content(Windows::Foundation::IInspectable const& value)
	{
		SetProperty(m_content, value, L"Content");
	}

	Windows::Foundation::IInspectable InfoBarOptions::ActionButtonContent() const
	{
		return m_actionButtonContent;
	}

	void InfoBarOptions::ActionButtonContent(Windows::Foundation::IInspectable const& value)
	{
		SetProperty(m_actionButtonContent, value, L"ActionButtonContent");
	}

	Microsoft::UI::Xaml::Input::ICommand InfoBarOptions::ActionButtonCommand() const
	{
		return m_actionButtonCommand;
	}

	void InfoBarOptions::ActionButtonCommand(Microsoft::UI::Xaml::Input::ICommand const& value)
	{
		SetProperty(m_actionButtonCommand, value, L"ActionButtonCommand");
	}

	std::uint32_t InfoBarOptions::MilliSecondsDelay() const noexcept
	{
		return m_milliSecondsDelay;
	}

	void InfoBarOptions::MilliSecondsDelay(std::uint32_t value) noexcept
	{
		SetProperty(m_milliSecondsDelay, value, L"MilliSecondsDelay");
	}

	bool InfoBarOptions::IsTextSelectionEnabled() const noexcept
	{
		return m_severity >= Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning;
	}
}
