module;
#include "XamlWorkaround.h"

export module OpenNet.Service.Notification.InfoBarService;

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.OpenNet.Service.Notification;
import winrt.Windows.Foundation.Collections;

export namespace OpenNet::Service::Notification
{
	struct InfoBarMessage
	{
		winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity Severity{ winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
		winrt::hstring Title;
		winrt::hstring Message;
		winrt::Windows::Foundation::IInspectable Content{ nullptr };
		winrt::Windows::Foundation::IInspectable ActionButtonContent{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand ActionButtonCommand{ nullptr };
		std::uint32_t DelayMilliseconds{ std::numeric_limits<std::uint32_t>::max() };

		static InfoBarMessage Error(winrt::hstring const& message);
		static InfoBarMessage Error(winrt::hstring const& title, winrt::hstring const& message);
		static InfoBarMessage Information(winrt::hstring const& message);
		static InfoBarMessage Success(winrt::hstring const& message);
		static InfoBarMessage Success(winrt::hstring const& title, winrt::hstring const& message);
		static InfoBarMessage Warning(winrt::hstring const& message);
		static InfoBarMessage Warning(winrt::hstring const& title, winrt::hstring const& message);
	};

	class IInfoBarService
	{
	public:
		using InfoBarOptions = winrt::OpenNet::Service::Notification::InfoBarOptions;
		using Collection = winrt::Windows::Foundation::Collections::IObservableVector<InfoBarOptions>;

		virtual ~IInfoBarService() = default;
		virtual Collection InfoBars() const = 0;
		virtual void Show(InfoBarMessage message) = 0;
		virtual void Show(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
			std::uint32_t delayMilliseconds = std::numeric_limits<std::uint32_t>::max(),
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
		void Show(InfoBarMessage message) override;
		void Show(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
			std::uint32_t delayMilliseconds = std::numeric_limits<std::uint32_t>::max(),
			winrt::hstring const& actionText = {},
			std::function<void()> action = {}) override;
		void Remove(InfoBarOptions const& options) override;

	private:
		void Enqueue(InfoBarMessage notification);
		void RemoveOnDispatcher(InfoBarOptions const& options);

		mutable std::mutex m_mutex;
		Collection m_infoBars{ nullptr };
		winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
		std::vector<InfoBarMessage> m_pending;
	};
}
