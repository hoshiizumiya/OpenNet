#include "XamlWorkaround.h"
#include "InfoBarOptions.h"
#if __has_include("Service/Notification/InfoBarOptions.g.cpp")
#include "Service/Notification/InfoBarOptions.g.cpp"
#endif

namespace winrt::OpenNet::Service::Notification::implementation
{
    Microsoft::UI::Xaml::Controls::InfoBarSeverity
        InfoBarOptions::Severity() const noexcept
    {
        return m_severity;
    }

    void InfoBarOptions::Severity(
        Microsoft::UI::Xaml::Controls::InfoBarSeverity value) noexcept
    {
        m_severity = value;
    }

    hstring InfoBarOptions::Title() const { return m_title; }
    void InfoBarOptions::Title(hstring const& value) { m_title = value; }
    hstring InfoBarOptions::Message() const { return m_message; }
    void InfoBarOptions::Message(hstring const& value) { m_message = value; }

    Windows::Foundation::IInspectable InfoBarOptions::Content() const
    {
        return m_content;
    }

    void InfoBarOptions::Content(
        Windows::Foundation::IInspectable const& value)
    {
        m_content = value;
    }

    Windows::Foundation::IInspectable
        InfoBarOptions::ActionButtonContent() const
    {
        return m_actionButtonContent;
    }

    void InfoBarOptions::ActionButtonContent(
        Windows::Foundation::IInspectable const& value)
    {
        m_actionButtonContent = value;
    }

    Microsoft::UI::Xaml::Input::ICommand
        InfoBarOptions::ActionButtonCommand() const
    {
        return m_actionButtonCommand;
    }

    void InfoBarOptions::ActionButtonCommand(
        Microsoft::UI::Xaml::Input::ICommand const& value)
    {
        m_actionButtonCommand = value;
    }

    std::uint32_t InfoBarOptions::MilliSecondsDelay() const noexcept
    {
        return m_milliSecondsDelay;
    }

    void InfoBarOptions::MilliSecondsDelay(std::uint32_t value) noexcept
    {
        m_milliSecondsDelay = value;
    }

    bool InfoBarOptions::IsTextSelectionEnabled() const noexcept
    {
        return m_severity >=
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning;
    }
}
