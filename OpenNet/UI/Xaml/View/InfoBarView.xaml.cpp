#include "XamlWorkaround.h"
#include "InfoBarView.xaml.h"
#if __has_include("UI/Xaml/View/InfoBarView.g.cpp")
#include "UI/Xaml/View/InfoBarView.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

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
		UnsubscribeInfoBars();
		StopTimers();
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

		auto weak = get_weak();
		m_infoBarsChangedToken = m_infoBars.VectorChanged(
			[weak](auto const& sender, auto const& args)
		{
			if (auto self = weak.get())
			{
				self->OnInfoBarsVectorChanged(sender, args);
			}
		});
		m_infoBarsSubscribed = true;
		Unloaded([weak](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				self->UnsubscribeInfoBars();
			}
		});
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
		try
		{
			if (auto options = sender.DataContext().try_as<
				OpenNet::Service::Notification::InfoBarOptions>())
			{
				Remove(options);
			}
		}
		catch (winrt::hresult_error const&)
		{
			// The visual tree may already be disconnected while the app exits.
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

	void InfoBarView::OnInfoBarsVectorChanged(
		Windows::Foundation::Collections::IObservableVector<
		OpenNet::Service::Notification::InfoBarOptions> const&,
		Windows::Foundation::Collections::IVectorChangedEventArgs const& args)
	{
		UpdateBadge();
		HandleInfoBarsCollectionChangedAsync(
			args.CollectionChange() ==
			Windows::Foundation::Collections::CollectionChange::ItemInserted);
	}

	void InfoBarView::UnsubscribeInfoBars()
	{
		if (!m_infoBarsSubscribed)
		{
			return;
		}
		m_infoBars.VectorChanged(m_infoBarsChangedToken);
		m_infoBarsSubscribed = false;
	}

	void InfoBarView::UpdateBadge()
	{
		if (NotificationCountBadge())
		{
			NotificationCountBadge().Value(
				static_cast<std::int32_t>(m_infoBars.Size()));
		}
	}

	winrt::fire_and_forget InfoBarView::HandleInfoBarsCollectionChangedAsync(
		bool added)
	{
		auto strong = get_strong();

		if (m_infoBars.Size() > 0)
		{
			VisibilityRoot().Visibility(Visibility::Visible);
		}

		auto transition = InfoBarPanelTransitionHelper();
		if (added)
		{
			if (transition)
			{
				transition.Source(ShowButtonBorder());
				transition.Target(InfoBarItemsBorder());

				if (VisualTreeHelper::GetParent(ShowButtonBorder()) &&
					VisualTreeHelper::GetParent(InfoBarItemsBorder()))
				{
					try
					{
						co_await transition.StartAsync();
					}
					catch (winrt::hresult_error const&)
					{
						// The app can disconnect the visual tree while exiting.
					}
				}
			}

			// An item can be removed while the expansion transition is running.
			if (m_infoBars.Size() > 0)
			{
				co_return;
			}
		}

		if (m_infoBars.Size() == 0)
		{
			if (transition)
			{
				transition.Source(InfoBarItemsBorder());
				transition.Target(ShowButtonBorder());

				if (VisualTreeHelper::GetParent(InfoBarItemsBorder()) &&
					VisualTreeHelper::GetParent(ShowButtonBorder()))
				{
					try
					{
						co_await transition.StartAsync();
					}
					catch (winrt::hresult_error const&)
					{
						// The app can disconnect the visual tree while exiting.
					}
				}
			}

			try
			{
				VisibilityRoot().Visibility(Visibility::Collapsed);
			}
			catch (winrt::hresult_error const&)
			{
				// The app can disconnect the visual tree while exiting.
			}
		}
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
				try
				{
					auto const count = self->m_infoBars.Size();
					if (count > 0)
					{
						self->Remove(self->m_infoBars.GetAt(count - 1));
					}
				}
				catch (winrt::hresult_error const&)
				{
					// The app can disconnect the visual tree while exiting.
					timer.Stop();
					self->m_clearTimer = nullptr;
					return;
				}
				if (self->m_infoBars.Size() == 0)
				{
					timer.Stop();
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
}
