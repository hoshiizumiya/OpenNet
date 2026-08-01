#pragma once

import winrt.Microsoft.UI.Composition;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Hosting;
import winrt.Windows.Foundation.Collections;
import winrt.Windows.Foundation.Numerics;

#include "Service/Notification/InfoBarOptions.h"
#include "UI/Xaml/View/InfoBarView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
    struct InfoBarView : InfoBarViewT<InfoBarView>
    {
        InfoBarView();
        ~InfoBarView();
        void InitializeComponent();

        winrt::Windows::Foundation::Collections::IObservableVector<
            OpenNet::Service::Notification::InfoBarOptions> InfoBars() const;

        static void Show(
            winrt::hstring const& title,
            winrt::hstring const& message,
            winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity =
                winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational,
            std::uint32_t delayMilliseconds = 6000,
            winrt::hstring const& actionText = {},
            std::function<void()> action = {});

        void OnInfoBarClosed(
            winrt::Microsoft::UI::Xaml::Controls::InfoBar const& sender,
            winrt::Microsoft::UI::Xaml::Controls::InfoBarClosedEventArgs const&);
        void OnClearAllButtonClick(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnCollapseButtonClick(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnShowButtonClick(
            winrt::Windows::Foundation::IInspectable const&,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        struct TimedInfoBar
        {
            OpenNet::Service::Notification::InfoBarOptions options{ nullptr };
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer timer{ nullptr };
        };

        void Enqueue(
            winrt::hstring const& title,
            winrt::hstring const& message,
            winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
            std::uint32_t delayMilliseconds,
            winrt::hstring const& actionText,
            std::function<void()> action);
        void Remove(
            OpenNet::Service::Notification::InfoBarOptions const& options);
        void RemoveTimer(
            OpenNet::Service::Notification::InfoBarOptions const& options);
        void UpdateBadge();
        void ExpandPanel(bool animate);
        void CollapsePanel();
        void DismissRoot();
        void AnimateSwap(
            winrt::Microsoft::UI::Xaml::FrameworkElement const& outgoing,
            winrt::Microsoft::UI::Xaml::FrameworkElement const& incoming);
        void ResetVisual(
            winrt::Microsoft::UI::Xaml::FrameworkElement const& element);
        void StopTimers();

        static inline winrt::weak_ref<InfoBarView> s_current;
		winrt::Windows::Foundation::Collections::IObservableVector<
            OpenNet::Service::Notification::InfoBarOptions> m_infoBars{
                winrt::single_threaded_observable_vector<
                    OpenNet::Service::Notification::InfoBarOptions>() };
        std::vector<TimedInfoBar> m_timers;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
            m_transitionTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
            m_clearTimer{ nullptr };
        std::uint64_t m_transitionVersion{};
        bool m_isExpanded{ true };
    };
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
    struct InfoBarView :
        InfoBarViewT<InfoBarView, implementation::InfoBarView>
    {
    };
}
