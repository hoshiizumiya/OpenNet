#include "XamlWorkaround.h"
#include "TaskHttpLogPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskHttpLogPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskHttpLogPage.g.cpp"
#endif
#include "ViewModels/DisplayItems.h"

import OpenNet.Core.DownloadManager;
import winrt.Windows.Globalization.DateTimeFormatting;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Navigation;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskHttpLogPage::~TaskHttpLogPage()
	{
	}

	void TaskHttpLogPage::InitializeComponent()
	{
		TaskHttpLogPageT::InitializeComponent();
		m_entries = single_threaded_observable_vector<Windows::Foundation::IInspectable>();
		LogListView().ItemsSource(m_entries);
		auto weak = get_weak();
		Unloaded([weak](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				self->m_isActive.store(false, std::memory_order_release);
				self->StopRefreshTimer();
				self->m_viewModel = nullptr;
			}
		});
	}

	void TaskHttpLogPage::OnNavigatedTo(NavigationEventArgs const& args)
	{
		m_isActive.store(true, std::memory_order_release);
		m_viewModel = args.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		m_timer = DispatcherTimer();
		m_timer.Interval(std::chrono::seconds(1));
		auto weak = get_weak();
		m_tickToken = m_timer.Tick([weak](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->Refresh();
		});
		m_timer.Start();
		Refresh();
	}

	void TaskHttpLogPage::OnNavigatedFrom(NavigationEventArgs const&)
	{
		m_isActive.store(false, std::memory_order_release);
		StopRefreshTimer();
		m_viewModel = nullptr;
	}

	void TaskHttpLogPage::StopRefreshTimer() noexcept
	{
		if (!m_timer) return;
		try
		{
			m_timer.Stop();
			if (m_tickToken.value) m_timer.Tick(m_tickToken);
		}
		catch (...)
		{
		}
		m_tickToken = {};
		m_timer = nullptr;
	}

	void TaskHttpLogPage::Refresh()
	{
		if (!m_isActive.load(std::memory_order_acquire)) return;
		auto const task = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
		auto const entries = task ? ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskLog(to_string(task.Gid())) : std::vector<::OpenNet::Core::HttpTaskLogEntry>{};
		auto const formatter = Windows::Globalization::DateTimeFormatting::DateTimeFormatter{ L"shortdate longtime" };
		for (std::uint32_t index = 0; index < entries.size(); ++index)
		{
			winrt::OpenNet::ViewModels::TrackerLogDisplayItem item{ nullptr };
			if (index < m_entries.Size()) item = m_entries.GetAt(index).try_as<winrt::OpenNet::ViewModels::TrackerLogDisplayItem>();
			if (!item)
			{
				item = make<winrt::OpenNet::ViewModels::implementation::TrackerLogDisplayItem>(); m_entries.Append(item);
			}
			item.Time(formatter.Format(clock::from_time_t(static_cast<std::time_t>(entries[index].timestamp))));
			item.Content(to_hstring(entries[index].content));
		}
		while (m_entries.Size() > entries.size()) m_entries.RemoveAtEnd();
		EmptyText().Visibility(entries.empty() ? Visibility::Visible : Visibility::Collapsed);
	}
}
