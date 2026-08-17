#include "XamlWorkaround.h"
#include "Service/Notification/InfoBarService.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
	struct ActionCommand : winrt::implements<ActionCommand, winrt::Microsoft::UI::Xaml::Input::ICommand>
	{
		explicit ActionCommand(std::function<void()> action)
			: m_action(std::move(action))
		{
		}

		bool CanExecute(Windows::Foundation::IInspectable const&) const
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

namespace OpenNet::Service::Notification
{
	InfoBarService& InfoBarService::Instance() noexcept
	{
		static InfoBarService instance;
		return instance;
	}

	void InfoBarService::AttachDispatcher(
		Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
	{
		std::vector<PendingInfoBar> pending;
		{
			std::scoped_lock lock(m_mutex);
			m_dispatcher = dispatcher;
			if (!m_infoBars)
			{
				m_infoBars = single_threaded_observable_vector<InfoBarOptions>();
			}
			pending.swap(m_pending);
		}

		for (auto& notification : pending)
		{
			Enqueue(std::move(notification));
		}
	}

	IInfoBarService::Collection InfoBarService::InfoBars() const
	{
		std::scoped_lock lock(m_mutex);
		return m_infoBars;
	}

	void InfoBarService::Show(
		hstring const& title,
		hstring const& message,
		InfoBarSeverity const severity,
		std::uint32_t const delayMilliseconds,
		hstring const& actionText,
		std::function<void()> action)
	{
		PendingInfoBar notification{
			title,
			message,
			severity,
			delayMilliseconds,
			actionText,
			std::move(action) };

		Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
		{
			std::scoped_lock lock(m_mutex);
			if (!m_dispatcher || !m_infoBars)
			{
				m_pending.emplace_back(std::move(notification));
				return;
			}
			dispatcher = m_dispatcher;
		}

		if (dispatcher.HasThreadAccess())
		{
			Enqueue(std::move(notification));
			return;
		}

		auto sharedNotification =
			std::make_shared<PendingInfoBar>(std::move(notification));
		if (!dispatcher.TryEnqueue([this, sharedNotification]() mutable
		{
			Enqueue(std::move(*sharedNotification));
		}))
		{
			std::scoped_lock lock(m_mutex);
			m_pending.emplace_back(std::move(*sharedNotification));
		}
	}

	void InfoBarService::Enqueue(PendingInfoBar notification)
	{
		Collection collection{ nullptr };
		{
			std::scoped_lock lock(m_mutex);
			collection = m_infoBars;
		}
		if (!collection)
		{
			std::scoped_lock lock(m_mutex);
			m_pending.emplace_back(std::move(notification));
			return;
		}

		InfoBarOptions options;
		options.Title(notification.Title);
		options.Message(notification.Message);
		options.Severity(notification.Severity);
		options.MilliSecondsDelay(notification.DelayMilliseconds);

		if (!notification.ActionText.empty() && notification.Action)
		{
			options.ActionButtonContent(box_value(notification.ActionText));
			auto weakOptions = make_weak(options);
			options.ActionButtonCommand(make<ActionCommand>(
				[this,
				 weakOptions,
				 action = std::move(notification.Action)]() mutable
			{
				action();
				if (auto item = weakOptions.get())
				{
					Remove(item);
				}
			}));
		}

		// Match the reference notification stack: newest notifications appear at
		// the top, while Clear All removes the oldest item from the tail first.
		collection.InsertAt(0, options);
	}

	void InfoBarService::Remove(InfoBarOptions const& options)
	{
		Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
		{
			std::scoped_lock lock(m_mutex);
			dispatcher = m_dispatcher;
		}
		if (!dispatcher || dispatcher.HasThreadAccess())
		{
			RemoveOnDispatcher(options);
			return;
		}

		dispatcher.TryEnqueue(
			[this, options]() { RemoveOnDispatcher(options); });
	}

	void InfoBarService::RemoveOnDispatcher(InfoBarOptions const& options)
	{
		Collection collection{ nullptr };
		{
			std::scoped_lock lock(m_mutex);
			collection = m_infoBars;
		}
		if (!collection)
		{
			return;
		}

		std::uint32_t index{};
		if (collection.IndexOf(options, index))
		{
			collection.RemoveAt(index);
		}
	}
}
