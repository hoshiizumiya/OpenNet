#include "pch.h"
#include "ViewModels/TasksViewModel.h"
#include "ViewModels/TasksViewModel.g.cpp"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include "Core/P2PManager.h"

using namespace std::string_literals;

namespace winrt::OpenNet::ViewModels::implementation
{
	TasksViewModel::TasksViewModel() : ::mvvm::view_model<TasksViewModel>(winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread())
	{
		m_tasks = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TaskViewModel>();

		// task star
		m_newCommand = winrt::make<mvvm::delegate_command<winrt::Windows::Foundation::IInspectable>>(
			[weak = get_weak()](winrt::Windows::Foundation::IInspectable const&)
			{
				if (auto self = weak.get())
				{
					// raise with empty hstring parameter (not IInspectable) so handler signature matches
					self->m_addTaskRequested(*self, winrt::hstring());
				}
			}
		);

		// 具有无法执行的问题
		m_startCommand = winrt::make<mvvm::delegate_command<winrt::Windows::Foundation::IInspectable>>(
			[](winrt::Windows::Foundation::IInspectable const&)
			{
				// run on background thread to avoid UI stalls; don't touch VM off-UI thread
				auto _ = []() -> winrt::fire_and_forget
					{
						try
						{
							co_await winrt::resume_background();
							auto& mgr = ::OpenNet::Core::P2PManager::Instance();
							if (auto core = mgr.TorrentCore())
							{
								core->Start();
							}
							else
							{
								co_await mgr.EnsureTorrentCoreInitializedAsync();
							}
						}
						catch (...)
						{ /* swallow */
						}
					}();
				(void)_; // silence unused warning
			}
		);

		m_pauseCommand = winrt::make<mvvm::delegate_command<winrt::Windows::Foundation::IInspectable>>(
			[](winrt::Windows::Foundation::IInspectable const&)
			{
				// run on background thread to avoid UI stalls; don't touch VM off-UI thread
				auto _ = []() -> winrt::fire_and_forget
					{
						try
						{
							co_await winrt::resume_background();
							if (auto core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
							{
								core->Stop();
							}
						}
						catch (...)
						{ /* swallow */
						}
					}();
				(void)_; // silence unused warning
			}
		);
		m_deleteCommand = winrt::make<mvvm::delegate_command<winrt::Windows::Foundation::IInspectable>>(
			[weak = get_weak()](winrt::Windows::Foundation::IInspectable const&)
			{
				if (auto self = weak.get())
				{
					auto dispatcher = self->m_dispatcher;
					if (!dispatcher) return;
					auto vec = self->m_tasks;
					dispatcher.TryEnqueue([vec]() mutable
										  {
											  if (vec)
											  {
												  vec.Clear();
											  }
										  });
				}
			}
		);

		// Register high-level callbacks once
		::OpenNet::Core::P2PManager::Instance().SetProgressCallback([weak = get_weak()](const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e)
																	{
																		if (auto self = weak.get()) self->OnProgress(e);
																	});
		::OpenNet::Core::P2PManager::Instance().SetFinishedCallback([weak = get_weak()](const std::string& name)
																	{
																		if (auto self = weak.get()) self->OnFinished(name);
																	});
		::OpenNet::Core::P2PManager::Instance().SetErrorCallback([weak = get_weak()](const std::string& msg)
																 {
																	 if (auto self = weak.get()) self->OnError(msg);
																 });

		Initialize();
	}

	void TasksViewModel::Initialize()
	{
		// trigger background core init (fire and forget)
		auto _ = ::OpenNet::Core::P2PManager::Instance().EnsureTorrentCoreInitializedAsync();
	}

	void TasksViewModel::Shutdown()
	{
		// no direct core ownership anymore
	}

	winrt::OpenNet::ViewModels::TaskViewModel TasksViewModel::FindOrCreateItem(winrt::hstring const& name)
	{
		for (auto const& item : m_tasks)
		{
			if (item.Name() == name)
			{
				return item;
			}
		}
		auto vm = winrt::make<winrt::OpenNet::ViewModels::implementation::TaskViewModel>();
		vm.Name(name);
		vm.AddDate(winrt::clock().now().time_since_epoch().count() ? L"" : L""); // placeholder
		m_tasks.Append(vm);
		return vm;
	}

	static winrt::hstring to_hstring_percent(int v)
	{
		wchar_t buf[32];
		swprintf(buf, 32, L"%d%%", v);
		return winrt::hstring{ buf };
	}

	static winrt::hstring to_hstring_rate(int kbps)
	{
		if (kbps > 1024)
		{
			wchar_t buf[64];
			swprintf(buf, 64, L"%.1f MB/s", kbps / 1024.0);
			return winrt::hstring{ buf };
		}
		wchar_t buf[64];
		swprintf(buf, 64, L"%d KB/s", kbps);
		return winrt::hstring{ buf };
	}

	void TasksViewModel::OnProgress(const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e)
	{
		auto dispatcher = m_dispatcher;
		if (!dispatcher) return;
		auto name = winrt::to_hstring(e.name);
		dispatcher.TryEnqueue([weak = get_weak(), name, e]()
							  {
								  if (auto self = weak.get())
								  {
									  auto item = self->FindOrCreateItem(name);
									  item.Progress(to_hstring_percent(e.progressPercent));
									  item.DownloadRate(to_hstring_rate(e.downloadRateKB));
									  if (item.Size().empty()) item.Size(L"-");
									  if (item.Remaining().empty()) item.Remaining(L"-");
								  }
							  });
	}

	void TasksViewModel::OnFinished(std::string const& name)
	{
		auto dispatcher = m_dispatcher;
		if (!dispatcher) return;
		auto hname = winrt::to_hstring(name);
		dispatcher.TryEnqueue([weak = get_weak(), hname]()
							  {
								  if (auto self = weak.get())
								  {
									  auto item = self->FindOrCreateItem(hname);
									  item.Progress(L"100%");
									  item.DownloadRate(L"0 KB/s");
									  item.Remaining(L"0");
								  }
							  });
	}

	void TasksViewModel::OnError(std::string const& msg)
	{
		(void)msg;
	}
}
