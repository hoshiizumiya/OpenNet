#include "XamlWorkaround.h"
#include "InfoOverlayWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/InfoOverlayWindow.g.cpp")
#include "UI/Xaml/View/Windows/InfoOverlayWindow.g.cpp"
#endif

import OpenNet.Core.DownloadManager;
import OpenNet.Core.P2PManager;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI.Windowing;
import winrt.Windows.Graphics;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	InfoOverlayWindow::InfoOverlayWindow()
	{
		InitializeComponent();
		InitializeWindowExBase();
		ExtendsContentIntoTitleBar(true);
		Closed([this](auto const&, auto const&)
		{
			if (m_refreshTimer)
			{
				m_refreshTimer.Stop();
				m_refreshTimer = nullptr;
			}
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(AppWindow());
		});
	}

	void InfoOverlayWindow::OverlayRoot_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (!m_refreshTimer)
		{
			m_refreshTimer = DispatcherQueue().CreateTimer();
			m_refreshTimer.Interval(std::chrono::seconds(1));
			auto weak = get_weak();
			m_refreshTimer.Tick([weak](auto const&, auto const&)
			{
				if (auto self = weak.get())
				{
					self->RefreshStatistics();
				}
			});
			m_refreshTimer.Start();
		}
		RefreshStatistics();
	}

	void InfoOverlayWindow::RefreshStatistics()
	{
		std::uint64_t downloadRate =
			::OpenNet::Core::DownloadManager::Instance().TotalHttpDownloadSpeed();
		std::uint64_t uploadRate =
			::OpenNet::Core::DownloadManager::Instance().TotalHttpUploadSpeed();
		int dhtNodes = 0;

		auto const statistics =
			::OpenNet::Core::P2PManager::Instance().GetPerformanceStats();
		downloadRate += static_cast<std::uint64_t>(
			std::max<std::int64_t>(0, statistics.totalDownloadRate));
		uploadRate += static_cast<std::uint64_t>(
			std::max<std::int64_t>(0, statistics.totalUploadRate));
		dhtNodes = statistics.dhtNodes;

		auto formatRate = [](std::uint64_t bytes) -> winrt::hstring
		{
			constexpr double kib = 1024.0;
			constexpr double mib = kib * 1024.0;
			wchar_t buffer[64]{};
			if (bytes >= static_cast<std::uint64_t>(mib))
			{
				swprintf_s(buffer, L"%.1f MiB/s", bytes / mib);
			}
			else if (bytes >= static_cast<std::uint64_t>(kib))
			{
				swprintf_s(buffer, L"%.1f KiB/s", bytes / kib);
			}
			else
			{
				swprintf_s(buffer, L"%llu B/s",
						   static_cast<unsigned long long>(bytes));
			}
			return buffer;
		};

		DownloadSpeedText().Text(formatRate(downloadRate));
		UploadSpeedText().Text(formatRate(uploadRate));
		DhtText().Text(winrt::hstring(
			L"DHT: " + std::to_wstring(dhtNodes) + L" nodes"));
	}
}
