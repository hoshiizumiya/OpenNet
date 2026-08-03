#pragma once

import std;
import winrt.Windows.Foundation;
import winrt.Windows.Media.Playback;
import winrt.Microsoft.UI;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;

namespace OpenNet::Service::Background
{
	enum class BackgroundSourceKind
	{
		Image,
		Video,
	};

	enum class BackgroundSourceMode
	{
		None = 0,
		SingleFile = 1,
		LocalFolder = 2,
	};

	struct BackgroundMediaOptions
	{
		BackgroundSourceMode ImageMode{ BackgroundSourceMode::None };
		BackgroundSourceMode VideoMode{ BackgroundSourceMode::None };
		winrt::hstring ImagePath;
		winrt::hstring VideoPath;
		std::int32_t ImageStretch{ 3 };
		std::int32_t VideoStretch{ 3 };
		double ImageOpacity{ 20.0 };
		double VideoOpacity{ 20.0 };
		bool VideoMuted{ true };
		bool VideoLooping{ true };
		std::int64_t RotationMinutes{ 5 };
	};

	class IBackgroundMediaService
	{
	public:
		virtual ~IBackgroundMediaService() = default;

		virtual BackgroundMediaOptions LoadOptions() const = 0;
		virtual void SaveOptions(BackgroundMediaOptions const& options) = 0;
		virtual winrt::event_token OptionsChanged(
			winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler) = 0;
		virtual void OptionsChanged(winrt::event_token const& token) noexcept = 0;
		virtual void NotifyOptionsChanged() = 0;

		virtual winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>
			PickSourceAsync(
				winrt::Microsoft::UI::WindowId windowId,
				BackgroundSourceKind kind,
				BackgroundSourceMode mode) = 0;

		virtual std::chrono::minutes RotationInterval() const = 0;

		virtual winrt::Windows::Foundation::IAsyncAction RefreshAsync(
			winrt::Microsoft::UI::Xaml::Controls::Image imagePresenter,
			winrt::Microsoft::UI::Xaml::Controls::MediaPlayerElement videoPresenter,
			bool advance) = 0;

		virtual void Suspend(
			winrt::Microsoft::UI::Xaml::Controls::MediaPlayerElement const& videoPresenter) noexcept = 0;

		virtual void Reset(
			winrt::Microsoft::UI::Xaml::Controls::Image const& imagePresenter,
			winrt::Microsoft::UI::Xaml::Controls::MediaPlayerElement const& videoPresenter) noexcept = 0;
	};
}
