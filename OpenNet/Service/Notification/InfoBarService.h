#pragma once

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Windows.Foundation.Collections;

#include "Service/Notification/InfoBarOptions.h"

namespace OpenNet::Service::Notification
{
	class IInfoBarService
	{
	public:
		using InfoBarOptions = winrt::OpenNet::Service::Notification::InfoBarOptions;
		using Collection = winrt::Windows::Foundation::Collections::IObservableVector<InfoBarOptions>;

		virtual ~IInfoBarService() = default;
		virtual Collection InfoBars() const = 0;
		virtual void Show(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
			std::uint32_t delayMilliseconds = 6000,
			winrt::hstring const& actionText = {},
			std::function<void()> action = {}) = 0;
		virtual void Remove(InfoBarOptions const& options) = 0;
	};

	class InfoBarService final : public IInfoBarService
	{
	public:
		static InfoBarService& Instance() noexcept;

		void AttachDispatcher(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);
		Collection InfoBars() const override;
		void Show(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
			std::uint32_t delayMilliseconds = 6000,
			winrt::hstring const& actionText = {},
			std::function<void()> action = {}) override;
		void Remove(InfoBarOptions const& options) override;

	private:
		struct PendingInfoBar
		{
			winrt::hstring Title;
			winrt::hstring Message;
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity Severity{};
			std::uint32_t DelayMilliseconds{};
			winrt::hstring ActionText;
			std::function<void()> Action;
		};

		void Enqueue(PendingInfoBar notification);
		void RemoveOnDispatcher(InfoBarOptions const& options);

		mutable std::mutex m_mutex;
		Collection m_infoBars{ nullptr };
		winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
		std::vector<PendingInfoBar> m_pending;
	};
}
