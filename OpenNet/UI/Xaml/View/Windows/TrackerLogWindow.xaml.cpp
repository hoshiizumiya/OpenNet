#include "XamlWorkaround.h"
import winrt.XamlToolkit.Labs.WinUI;
#include "TrackerLogWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/TrackerLogWindow.g.cpp")
#include "UI/Xaml/View/Windows/TrackerLogWindow.g.cpp"
#endif
#include "ViewModels/DisplayItems.h"

import OpenNet.Core.P2PManager;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI.Windowing;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Windows.Globalization.DateTimeFormatting;
import winrt.Windows.Graphics;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	TrackerLogWindow::TrackerLogWindow()
	{
		InitializeComponent();
		InitializeWindow();
	}

	TrackerLogWindow::TrackerLogWindow(
		winrt::hstring const& taskId,
		winrt::hstring const& taskName,
		winrt::hstring const& trackerUrl)
		: m_taskId(taskId), m_trackerUrl(trackerUrl)
	{
		InitializeComponent();
		TaskNameText().Text(taskName);
		TrackerUrlText().Text(trackerUrl);
		WindowTitleBar().Subtitle(trackerUrl);
		InitializeWindow();
	}

	void TrackerLogWindow::InitializeWindow()
	{
		m_entries = single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();
		LogListView().ItemsSource(m_entries);
		ExtendsContentIntoTitleBar(true);
		::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(*this);
		SetTitleBar(WindowTitleBar());
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(*this);

		m_refreshTimer = DispatcherTimer();
		m_refreshTimer.Interval(std::chrono::milliseconds(500));
		m_timerToken = m_refreshTimer.Tick([weak = get_weak()](auto&&, auto&&)
		{
			if (auto self = weak.get())
				self->RefreshLog();
		});
		m_refreshTimer.Start();
		Closed([this](auto&&, auto&&)
		{
			auto strong = this->get_strong();
			if (strong && strong->m_refreshTimer)
			{
				strong->m_refreshTimer.Stop();
				strong->m_refreshTimer.Tick(strong->m_timerToken);
			}
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(*this);
		});
		RefreshLog();
	}

	void TrackerLogWindow::RefreshLog()
	{
		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		if (m_taskId.empty() || m_trackerUrl.empty()
			|| !p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
		{
			EmptyLogText().Visibility(Visibility::Visible);
			return;
		}

		auto const entries = p2p.TorrentCore()->GetTrackerLog(
			winrt::to_string(m_taskId), winrt::to_string(m_trackerUrl));
		auto const formatter = winrt::Windows::Globalization::
			DateTimeFormatting::DateTimeFormatter{ L"shortdate longtime" };
		for (std::uint32_t index = 0; index < entries.size(); ++index)
		{
			winrt::OpenNet::ViewModels::TrackerLogDisplayItem item{ nullptr };
			if (index < m_entries.Size())
				item = m_entries.GetAt(index).try_as<
				winrt::OpenNet::ViewModels::TrackerLogDisplayItem>();
			if (!item)
			{
				item = winrt::make<winrt::OpenNet::ViewModels::implementation::
					TrackerLogDisplayItem>();
				m_entries.Append(item);
			}
			item.Time(formatter.Format(winrt::clock::from_time_t(
				static_cast<std::time_t>(entries[index].timestamp))));
			item.Content(winrt::to_hstring(entries[index].content));
			item.IsError(entries[index].isError);
		}
		while (m_entries.Size() > entries.size())
			m_entries.RemoveAtEnd();
		EmptyLogText().Visibility(
			m_entries.Size() == 0 ? Visibility::Visible : Visibility::Collapsed);
	}

	void TrackerLogWindow::AlwaysOnTopToggle_Toggled(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (auto presenter = AppWindow().Presenter().try_as<
			winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
		{
			presenter.IsAlwaysOnTop(AlwaysOnTopToggle().IsOn());
		}
	}
}
