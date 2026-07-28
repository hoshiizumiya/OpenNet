#include "XamlWorkaround.h"
#include "InfoBarView.xaml.h"
#if __has_include("UI/Xaml/View/InfoBarView.g.cpp")
#include "UI/Xaml/View/InfoBarView.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	InfoBarView::InfoBarView()
	{
		s_current = this;
	}

	InfoBarView::~InfoBarView()
	{
		for (auto const& timer : m_timers)
			timer.Stop();
		m_timers.clear();
		if (s_current == this)
			s_current = nullptr;
	}

	void InfoBarView::InitializeComponent()
	{
		InfoBarViewT::InitializeComponent();
		UpdateVisibility();
	}

	void InfoBarView::Show(
		hstring const& title,
		hstring const& message,
		InfoBarSeverity severity,
		std::uint32_t delayMilliseconds,
		hstring const& actionText,
		std::function<void()> action)
	{
		auto* current = s_current;
		if (!current)
			return;
		if (current->DispatcherQueue().HasThreadAccess())
		{
			current->Enqueue(
				title, message, severity, delayMilliseconds,
				actionText, std::move(action));
			return;
		}
		auto weak = current->get_weak();
		current->DispatcherQueue().TryEnqueue(
			[weak, title, message, severity, delayMilliseconds,
			actionText, action = std::move(action)]() mutable
		{
			if (auto self = weak.get())
				self->Enqueue(
					title, message, severity, delayMilliseconds,
					actionText, std::move(action));
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
		InfoBar infoBar;
		infoBar.Title(title);
		infoBar.Message(message);
		infoBar.Severity(severity);
		infoBar.IsClosable(true);
		infoBar.IsOpen(true);
		infoBar.HorizontalAlignment(HorizontalAlignment::Stretch);
		if (!actionText.empty() && action)
		{
			Button actionButton;
			actionButton.Content(box_value(actionText));
			actionButton.Click(
				[action = std::move(action), infoBar](
					auto const&, auto const&) mutable
			{
				action();
				infoBar.IsOpen(false);
			});
			infoBar.ActionButton(actionButton);
		}
		auto weak = get_weak();
		infoBar.Closed([weak, infoBar](auto const&, auto const&)
		{
			if (auto self = weak.get())
				self->Remove(infoBar);
		});
		InfoBarItemsPanel().Children().Append(infoBar);

		if (delayMilliseconds > 0)
		{
			auto timer = DispatcherQueue().CreateTimer();
			timer.IsRepeating(false);
			timer.Interval(std::chrono::milliseconds(delayMilliseconds));
			timer.Tick([weak, infoBar](auto const& sender, auto const&)
			{
				auto source = sender.template try_as<
					winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer>();
				if (source)
					source.Stop();
				if (auto self = weak.get())
				{
					if (source)
					{
						std::erase(self->m_timers, source);
					}
					infoBar.IsOpen(false);
				}
			});
			m_timers.push_back(timer);
			timer.Start();
		}
		InfoBarItemsBorder().Visibility(Visibility::Visible);
		ShowButtonBorder().Visibility(Visibility::Collapsed);
		UpdateVisibility();
	}

	void InfoBarView::Remove(InfoBar const& infoBar)
	{
		std::uint32_t index{};
		if (InfoBarItemsPanel().Children().IndexOf(infoBar, index))
			InfoBarItemsPanel().Children().RemoveAt(index);
		UpdateVisibility();
	}

	void InfoBarView::UpdateVisibility()
	{
		auto const count = InfoBarItemsPanel().Children().Size();
		NotificationCountBadge().Value(static_cast<int>(count));
		VisibilityRoot().Visibility(
			count == 0 ? Visibility::Collapsed : Visibility::Visible);
		if (count == 0)
		{
			InfoBarItemsBorder().Visibility(Visibility::Collapsed);
			ShowButtonBorder().Visibility(Visibility::Collapsed);
		}
	}

	void InfoBarView::OnClearAllButtonClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		for (auto const& timer : m_timers)
			timer.Stop();
		m_timers.clear();
		InfoBarItemsPanel().Children().Clear();
		UpdateVisibility();
	}

	void InfoBarView::OnCollapseButtonClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		InfoBarItemsBorder().Visibility(Visibility::Collapsed);
		ShowButtonBorder().Visibility(Visibility::Visible);
	}

	void InfoBarView::OnShowButtonClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		ShowButtonBorder().Visibility(Visibility::Collapsed);
		InfoBarItemsBorder().Visibility(Visibility::Visible);
	}
}
