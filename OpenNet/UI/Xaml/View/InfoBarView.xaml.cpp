#include "XamlWorkaround.h"
#include "InfoBarView.xaml.h"
#if __has_include("UI/Xaml/View/InfoBarView.g.cpp")
#include "UI/Xaml/View/InfoBarView.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Hosting;

namespace
{
    struct ActionCommand :
        winrt::implements<
            ActionCommand,
            winrt::Microsoft::UI::Xaml::Input::ICommand>
    {
        explicit ActionCommand(std::function<void()> action)
            : m_action(std::move(action))
        {
        }

        bool CanExecute(Windows::Foundation::IInspectable const&)
        {
            return static_cast<bool>(m_action);
        }

        void Execute(Windows::Foundation::IInspectable const&)
        {
            if (m_action)
            {
                m_action();
            }
        }

        event_token CanExecuteChanged(
            Windows::Foundation::EventHandler<
                Windows::Foundation::IInspectable> const& handler)
        {
            return m_canExecuteChanged.add(handler);
        }

        void CanExecuteChanged(event_token const& token) noexcept
        {
            m_canExecuteChanged.remove(token);
        }

    private:
        std::function<void()> m_action;
        event<Windows::Foundation::EventHandler<
            Windows::Foundation::IInspectable>> m_canExecuteChanged;
    };
}

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
    InfoBarView::InfoBarView() = default;

    InfoBarView::~InfoBarView()
    {
        StopTimers();
        if (m_transitionTimer)
        {
            m_transitionTimer.Stop();
        }
        if (m_clearTimer)
        {
            m_clearTimer.Stop();
        }
    }

    void InfoBarView::InitializeComponent()
    {
        InfoBarViewT::InitializeComponent();
        s_current = get_weak();
        UpdateBadge();
    }

    Windows::Foundation::Collections::IObservableVector<
        OpenNet::Service::Notification::InfoBarOptions>
        InfoBarView::InfoBars() const
    {
        return m_infoBars;
    }

    void InfoBarView::Show(
        hstring const& title,
        hstring const& message,
        InfoBarSeverity severity,
        std::uint32_t delayMilliseconds,
        hstring const& actionText,
        std::function<void()> action)
    {
        auto current = s_current.get();
        if (!current)
        {
            return;
        }

        if (current->DispatcherQueue().HasThreadAccess())
        {
            current->Enqueue(
                title,
                message,
                severity,
                delayMilliseconds,
                actionText,
                std::move(action));
            return;
        }

        auto weak = current->get_weak();
        current->DispatcherQueue().TryEnqueue(
            [weak,
             title,
             message,
             severity,
             delayMilliseconds,
             actionText,
             action = std::move(action)]() mutable
            {
                if (auto self = weak.get())
                {
                    self->Enqueue(
                        title,
                        message,
                        severity,
                        delayMilliseconds,
                        actionText,
                        std::move(action));
                }
            });
    }

    void InfoBarView::Enqueue(
        hstring const& title,
        hstring const& message,
        InfoBarSeverity severity,
        std::uint32_t delayMilliseconds,
        hstring const& actionText,
        std::function<void()> action)
    {
        if (m_clearTimer)
        {
            m_clearTimer.Stop();
        }

        OpenNet::Service::Notification::InfoBarOptions options;
        options.Title(title);
        options.Message(message);
        options.Severity(severity);
        options.MilliSecondsDelay(delayMilliseconds);

        if (!actionText.empty() && action)
        {
            options.ActionButtonContent(box_value(actionText));
            auto weakView = get_weak();
            auto weakOptions = make_weak(options);
            options.ActionButtonCommand(
                make<ActionCommand>(
                    [weakView,
                     weakOptions,
                     action = std::move(action)]() mutable
                    {
                        action();
                        if (auto self = weakView.get())
                        {
                            if (auto item = weakOptions.get())
                            {
                                self->Remove(item);
                            }
                        }
                    }));
        }

        m_infoBars.Append(options);
        UpdateBadge();
        VisibilityRoot().Visibility(Visibility::Visible);
        ExpandPanel(true);

        if (delayMilliseconds > 0)
        {
            auto timer = DispatcherQueue().CreateTimer();
            timer.IsRepeating(false);
            timer.Interval(std::chrono::milliseconds(delayMilliseconds));
            auto weak = get_weak();
            auto weakOptions = make_weak(options);
            timer.Tick(
                [weak, weakOptions](auto const& sender, auto const&)
                {
                    auto source = sender.template try_as<
                        Microsoft::UI::Dispatching::DispatcherQueueTimer>();
                    if (source)
                    {
                        source.Stop();
                    }
                    if (auto self = weak.get())
                    {
                        if (auto item = weakOptions.get())
                        {
                            self->Remove(item);
                        }
                    }
                });
            m_timers.push_back({ options, timer });
            timer.Start();
        }
    }

    void InfoBarView::OnInfoBarClosed(
        InfoBar const& sender,
        InfoBarClosedEventArgs const&)
    {
        if (auto options = sender.DataContext().try_as<
            OpenNet::Service::Notification::InfoBarOptions>())
        {
            Remove(options);
        }
    }

    void InfoBarView::Remove(
        OpenNet::Service::Notification::InfoBarOptions const& options)
    {
        RemoveTimer(options);

        std::uint32_t index{};
        if (!m_infoBars.IndexOf(options, index))
        {
            return;
        }

        m_infoBars.RemoveAt(index);
        UpdateBadge();
        if (m_infoBars.Size() == 0)
        {
            DismissRoot();
        }
    }

    void InfoBarView::RemoveTimer(
        OpenNet::Service::Notification::InfoBarOptions const& options)
    {
        for (auto iterator = m_timers.begin();
             iterator != m_timers.end();)
        {
            if (iterator->options == options)
            {
                iterator->timer.Stop();
                iterator = m_timers.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }

    void InfoBarView::UpdateBadge()
    {
        if (NotificationCountBadge())
        {
            NotificationCountBadge().Value(
                static_cast<std::int32_t>(m_infoBars.Size()));
        }
    }

    void InfoBarView::ExpandPanel(bool animate)
    {
        m_isExpanded = true;
        VisibilityRoot().Visibility(Visibility::Visible);
        if (m_transitionTimer)
        {
            m_transitionTimer.Stop();
            ++m_transitionVersion;
            ResetVisual(ShowButtonBorder());
            ResetVisual(InfoBarItemsBorder());
        }
        if (!animate)
        {
            ShowButtonBorder().Visibility(Visibility::Collapsed);
            InfoBarItemsBorder().Visibility(Visibility::Visible);
            ResetVisual(ShowButtonBorder());
            ResetVisual(InfoBarItemsBorder());
            return;
        }

        if (InfoBarItemsBorder().Visibility() != Visibility::Visible ||
            ShowButtonBorder().Visibility() == Visibility::Visible)
        {
            AnimateSwap(ShowButtonBorder(), InfoBarItemsBorder());
        }
    }

    void InfoBarView::CollapsePanel()
    {
        if (!m_isExpanded || m_infoBars.Size() == 0)
        {
            return;
        }
        m_isExpanded = false;
        AnimateSwap(InfoBarItemsBorder(), ShowButtonBorder());
    }

    void InfoBarView::DismissRoot()
    {
        ++m_transitionVersion;
        auto const version = m_transitionVersion;
        if (m_transitionTimer)
        {
            m_transitionTimer.Stop();
        }

        FrameworkElement outgoing = m_isExpanded
            ? InfoBarItemsBorder().as<FrameworkElement>()
            : ShowButtonBorder().as<FrameworkElement>();
        auto visual = ElementCompositionPreview::GetElementVisual(outgoing);
        auto compositor = visual.Compositor();
        ElementCompositionPreview::SetIsTranslationEnabled(outgoing, true);

        auto opacity = compositor.CreateScalarKeyFrameAnimation();
        opacity.InsertKeyFrame(1.0f, 0.0f);
        opacity.Duration(std::chrono::milliseconds(180));
        visual.StartAnimation(L"Opacity", opacity);

        auto translation = compositor.CreateScalarKeyFrameAnimation();
        translation.InsertKeyFrame(1.0f, 16.0f);
        translation.Duration(std::chrono::milliseconds(180));
        visual.StartAnimation(L"Translation.Y", translation);

        auto weak = get_weak();
        m_transitionTimer = DispatcherQueue().CreateTimer();
        m_transitionTimer.IsRepeating(false);
        m_transitionTimer.Interval(std::chrono::milliseconds(190));
        m_transitionTimer.Tick(
            [weak, version, outgoing](auto const& sender, auto const&)
            {
                sender.as<Microsoft::UI::Dispatching::DispatcherQueueTimer>()
                    .Stop();
                if (auto self = weak.get())
                {
                    if (self->m_transitionVersion != version ||
                        self->m_infoBars.Size() != 0)
                    {
                        return;
                    }
                    self->ResetVisual(outgoing);
                    self->InfoBarItemsBorder().Visibility(Visibility::Collapsed);
                    self->ShowButtonBorder().Visibility(Visibility::Collapsed);
                    self->VisibilityRoot().Visibility(Visibility::Collapsed);
                    self->m_transitionTimer = nullptr;
                }
            });
        m_transitionTimer.Start();
    }

    void InfoBarView::AnimateSwap(
        FrameworkElement const& outgoing,
        FrameworkElement const& incoming)
    {
        ++m_transitionVersion;
        auto const version = m_transitionVersion;
        if (m_transitionTimer)
        {
            m_transitionTimer.Stop();
        }

        incoming.Visibility(Visibility::Visible);
        ElementCompositionPreview::SetIsTranslationEnabled(outgoing, true);
        ElementCompositionPreview::SetIsTranslationEnabled(incoming, true);

        auto outgoingVisual =
            ElementCompositionPreview::GetElementVisual(outgoing);
        auto incomingVisual =
            ElementCompositionPreview::GetElementVisual(incoming);
        auto compositor = incomingVisual.Compositor();
        auto easing = compositor.CreateCubicBezierEasingFunction(
            { 0.0f, 0.0f }, { 0.2f, 1.0f });

        incomingVisual.Opacity(0.0f);
        incomingVisual.Properties().InsertScalar(L"Translation.X", 20.0f);

        auto incomingOpacity = compositor.CreateScalarKeyFrameAnimation();
        incomingOpacity.InsertKeyFrame(1.0f, 1.0f, easing);
        incomingOpacity.Duration(std::chrono::milliseconds(200));
        incomingVisual.StartAnimation(L"Opacity", incomingOpacity);

        auto incomingTranslation = compositor.CreateScalarKeyFrameAnimation();
        incomingTranslation.InsertKeyFrame(1.0f, 0.0f, easing);
        incomingTranslation.Duration(std::chrono::milliseconds(200));
        incomingVisual.StartAnimation(
            L"Translation.X", incomingTranslation);

        auto outgoingOpacity = compositor.CreateScalarKeyFrameAnimation();
        outgoingOpacity.InsertKeyFrame(1.0f, 0.0f, easing);
        outgoingOpacity.Duration(std::chrono::milliseconds(160));
        outgoingVisual.StartAnimation(L"Opacity", outgoingOpacity);

        auto outgoingTranslation = compositor.CreateScalarKeyFrameAnimation();
        outgoingTranslation.InsertKeyFrame(1.0f, -12.0f, easing);
        outgoingTranslation.Duration(std::chrono::milliseconds(160));
        outgoingVisual.StartAnimation(
            L"Translation.X", outgoingTranslation);

        auto weak = get_weak();
        m_transitionTimer = DispatcherQueue().CreateTimer();
        m_transitionTimer.IsRepeating(false);
        m_transitionTimer.Interval(std::chrono::milliseconds(210));
        m_transitionTimer.Tick(
            [weak, version, outgoing, incoming](
                auto const& sender,
                auto const&)
            {
                sender.as<Microsoft::UI::Dispatching::DispatcherQueueTimer>()
                    .Stop();
                if (auto self = weak.get())
                {
                    if (self->m_transitionVersion != version)
                    {
                        return;
                    }
                    outgoing.Visibility(Visibility::Collapsed);
                    self->ResetVisual(outgoing);
                    self->ResetVisual(incoming);
                    self->m_transitionTimer = nullptr;
                }
            });
        m_transitionTimer.Start();
    }

    void InfoBarView::ResetVisual(FrameworkElement const& element)
    {
        if (!element)
        {
            return;
        }
        auto visual = ElementCompositionPreview::GetElementVisual(element);
        visual.StopAnimation(L"Opacity");
        visual.StopAnimation(L"Translation.X");
        visual.StopAnimation(L"Translation.Y");
        visual.Opacity(1.0f);
        visual.Properties().InsertScalar(L"Translation.X", 0.0f);
        visual.Properties().InsertScalar(L"Translation.Y", 0.0f);
    }

    void InfoBarView::StopTimers()
    {
        for (auto const& item : m_timers)
        {
            item.timer.Stop();
        }
        m_timers.clear();
    }

    void InfoBarView::OnClearAllButtonClick(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        StopTimers();
        if (m_clearTimer)
        {
            m_clearTimer.Stop();
        }

        auto weak = get_weak();
        m_clearTimer = DispatcherQueue().CreateTimer();
        m_clearTimer.IsRepeating(true);
        m_clearTimer.Interval(std::chrono::milliseconds(50));
        m_clearTimer.Tick(
            [weak](auto const& sender, auto const&)
            {
                auto timer = sender.as<
                    Microsoft::UI::Dispatching::DispatcherQueueTimer>();
                if (auto self = weak.get())
                {
                    auto const count = self->m_infoBars.Size();
                    if (count > 0)
                    {
                        self->m_infoBars.RemoveAt(count - 1);
                        self->UpdateBadge();
                    }
                    if (self->m_infoBars.Size() == 0)
                    {
                        timer.Stop();
                        self->DismissRoot();
                        self->m_clearTimer = nullptr;
                    }
                }
                else
                {
                    timer.Stop();
                }
            });
        m_clearTimer.Start();
    }

    void InfoBarView::OnCollapseButtonClick(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        CollapsePanel();
    }

    void InfoBarView::OnShowButtonClick(
        Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        ExpandPanel(true);
    }
}
