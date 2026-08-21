module;

module OpenNet.Service.Notification.InfoBarService;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
	std::uint32_t EstimateReadingTime(winrt::hstring const& text)
	{
		std::size_t cjkCount{};
		std::size_t englishWordCount{};
		bool inEnglishWord{};
		for (auto const value : text)
		{
			if (value >= 0x4e00 && value <= 0x9fff) ++cjkCount;
			auto const isEnglishLetter = (value >= L'A' && value <= L'Z') || (value >= L'a' && value <= L'z');
			if (isEnglishLetter && !inEnglishWord) ++englishWordCount;
			inEnglishWord = isEnglishLetter;
		}
		auto const seconds = static_cast<double>(cjkCount) / 250.0 * 60.0 + static_cast<double>(englishWordCount) / 200.0 * 60.0;
		return static_cast<std::uint32_t>(std::ceil(seconds * 1000.0));
	}

	std::uint32_t DefaultDelay(InfoBarSeverity const severity, winrt::hstring const& title, winrt::hstring const& message)
	{
		if (severity == InfoBarSeverity::Error) return 0;
		return 5000 + EstimateReadingTime(title) + EstimateReadingTime(message);
	}

	struct ActionCommand : winrt::implements<ActionCommand, winrt::Microsoft::UI::Xaml::Input::ICommand>
	{
		explicit ActionCommand(std::function<void()> action) : m_action(std::move(action))
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

		event_token CanExecuteChanged(Windows::Foundation::EventHandler<Windows::Foundation::IInspectable> const& handler)
		{
			return m_canExecuteChanged.add(handler);
		}

		void CanExecuteChanged(event_token const& token) noexcept
		{
			m_canExecuteChanged.remove(token);
		}

	private:
		std::function<void()> m_action;
		event<Windows::Foundation::EventHandler<Windows::Foundation::IInspectable>> m_canExecuteChanged;
	};
}

namespace OpenNet::Service::Notification
{
	InfoBarMessage InfoBarMessage::Error(hstring const& message)
	{
		return { InfoBarSeverity::Error, {}, message, nullptr, nullptr, nullptr, 0 };
	}

	InfoBarMessage InfoBarMessage::Error(hstring const& title, hstring const& message)
	{
		return { InfoBarSeverity::Error, title, message, nullptr, nullptr, nullptr, 0 };
	}

	InfoBarMessage InfoBarMessage::Information(hstring const& message)
	{
		return { InfoBarSeverity::Informational, {}, message };
	}

	InfoBarMessage InfoBarMessage::Success(hstring const& message)
	{
		return { InfoBarSeverity::Success, {}, message };
	}

	InfoBarMessage InfoBarMessage::Success(hstring const& title, hstring const& message)
	{
		return { InfoBarSeverity::Success, title, message };
	}

	InfoBarMessage InfoBarMessage::Warning(hstring const& message)
	{
		return { InfoBarSeverity::Warning, {}, message };
	}

	InfoBarMessage InfoBarMessage::Warning(hstring const& title, hstring const& message)
	{
		return { InfoBarSeverity::Warning, title, message };
	}

	InfoBarService& InfoBarService::Instance() noexcept
	{
		static InfoBarService instance;
		return instance;
	}

	void InfoBarService::AttachDispatcher(Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
	{
		std::vector<InfoBarMessage> pending;
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

	void InfoBarService::Show(InfoBarMessage message)
	{
		if (message.DelayMilliseconds == std::numeric_limits<std::uint32_t>::max())
			message.DelayMilliseconds = DefaultDelay(message.Severity, message.Title, message.Message);

		Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
		{
			std::scoped_lock lock(m_mutex);
			if (!m_dispatcher || !m_infoBars)
			{
				m_pending.emplace_back(std::move(message));
				return;
			}
			dispatcher = m_dispatcher;
		}

		if (dispatcher.HasThreadAccess())
		{
			Enqueue(std::move(message));
			return;
		}

		auto sharedMessage = std::make_shared<InfoBarMessage>(std::move(message));
		if (!dispatcher.TryEnqueue([this, sharedMessage]() mutable
		{
			Enqueue(std::move(*sharedMessage));
		}))
		{
			std::scoped_lock lock(m_mutex);
			m_pending.emplace_back(std::move(*sharedMessage));
		}
	}

	void InfoBarService::Show(
		hstring const& title,
		hstring const& message,
		InfoBarSeverity const severity,
		std::uint32_t const delayMilliseconds,
		hstring const& actionText,
		std::function<void()> action)
	{
		InfoBarMessage notification{ severity, title, message };
		notification.DelayMilliseconds = delayMilliseconds;
		if (!actionText.empty() && action)
		{
			notification.ActionButtonContent = box_value(actionText);
			notification.ActionButtonCommand = make<ActionCommand>(std::move(action));
		}
		Show(std::move(notification));
	}

	void InfoBarService::Enqueue(InfoBarMessage notification)
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
		options.Content(notification.Content);
		options.ActionButtonContent(notification.ActionButtonContent);
		options.ActionButtonCommand(notification.ActionButtonCommand);
		options.MilliSecondsDelay(notification.DelayMilliseconds);

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

		dispatcher.TryEnqueue([this, options]()
		{
			RemoveOnDispatcher(options);
		});
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
