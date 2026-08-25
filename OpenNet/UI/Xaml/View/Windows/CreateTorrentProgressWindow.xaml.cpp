#include "XamlWorkaround.h"
#include "CreateTorrentProgressWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/CreateTorrentProgressWindow.g.cpp")
#include "UI/Xaml/View/Windows/CreateTorrentProgressWindow.g.cpp"
#endif

#include <shellapi.h>

import OpenNet.Core.P2PManager;
import OpenNet.Core.Utils.Message;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI.Windowing;
import winrt.Windows.Graphics;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	CreateTorrentProgressWindow::CreateTorrentProgressWindow()
	{
		InitializeComponent();
		InitializeWindowExBase();
		InitializeWindow();
	}

	CreateTorrentProgressWindow::CreateTorrentProgressWindow(::OpenNet::Core::Torrent::TorrentCreationOptions options, std::filesystem::path targetPath, bool const startSeeding)
		: m_options(std::move(options)), m_targetPath(std::move(targetPath)), m_startSeeding(startSeeding)
	{
		InitializeComponent();
		InitializeWindowExBase();
		InitializeWindow();
	}

	void CreateTorrentProgressWindow::InitializeWindow()
	{
		SetTitleBar(WindowTitleBar());
		ExtendsContentIntoTitleBar(true);
		AppWindow().TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);

		Closed([weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->m_stopSource.request_stop();
		});
	}

	void CreateTorrentProgressWindow::RootGrid_Loaded(IInspectable const&, RoutedEventArgs const&)
	{
		if (m_started) return;
		m_started = true;
		SourcePathText().Text(hstring{ m_options.sourcePath.c_str() });
		TargetPathText().Text(hstring{ m_targetPath.c_str() });
		StatusText().Text(ResourceGetString(L"CreateTorrentCreating"));
		CancelButton().Content(box_value(ResourceGetString(L"CommonCancel")));
		m_creationAction = StartCreationAsync();
	}

	winrt::Windows::Foundation::IAsyncAction CreateTorrentProgressWindow::StartCreationAsync()
	{
		auto lifetime = get_strong();
		auto dispatcher = DispatcherQueue();
		auto weak = get_weak();
		winrt::hstring failure;
		bool seedingStarted{};
		try
		{
			co_await winrt::resume_background();
			auto result = ::OpenNet::Core::Torrent::TorrentCreator::Create(m_options, [dispatcher, weak](int const completedPieces, int const totalPieces)
			{
				(void)dispatcher.TryEnqueue([weak, completedPieces, totalPieces]()
				{
					if (auto self = weak.get()) self->UpdateProgress(completedPieces, totalPieces);
				});
			}, m_stopSource.get_token());
			::OpenNet::Core::Torrent::TorrentCreator::WriteFile(m_targetPath, result);
			if (m_startSeeding && !m_stopSource.stop_requested())
			{
				seedingStarted = co_await ::OpenNet::Core::P2PManager::Instance().AddTorrentFileAsync(m_targetPath.string(), m_options.sourcePath.parent_path().string(), {}, m_options.trackers, true, true);
			}
		}
		catch (std::exception const& error)
		{
			failure = to_hstring(error.what());
		}
		catch (...)
		{
			failure = ResourceGetString(L"CreateTorrentUnknownFailure");
		}
		co_await winrtplus::resume_foreground(dispatcher);
		if (failure.empty())
		{
			UpdateProgress(1, 1);
			Complete(m_startSeeding && !seedingStarted ? ResourceGetString(L"CreateTorrentCompleteSeedingFailed") : ResourceGetString(L"CreateTorrentComplete"), true);
		}
		else
		{
			Complete(ResourceGetString(L"CreateTorrentFailed") + L" " + failure, false);
		}
	}

	void CreateTorrentProgressWindow::UpdateProgress(int const completedPieces, int const totalPieces)
	{
		auto const progress = totalPieces > 0 ? std::clamp(static_cast<double>(completedPieces) / totalPieces, 0.0, 1.0) : 0.0;
		CreationProgressBar().Value(progress);
		ProgressPercentText().Text(std::format(L"{:.0f}%", progress * 100.0));
		StatusText().Text(std::format(L"{} / {}", completedPieces, totalPieces));
	}

	void CreateTorrentProgressWindow::Complete(hstring const& message, bool const success)
	{
		m_completed = true;
		StatusText().Text(message);
		CancelButton().Visibility(Visibility::Collapsed);
		CloseButton().Visibility(Visibility::Visible);
		OpenFolderButton().Visibility(success ? Visibility::Visible : Visibility::Collapsed);
	}

	void CreateTorrentProgressWindow::CancelButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		m_stopSource.request_stop();
		CancelButton().IsEnabled(false);
		StatusText().Text(ResourceGetString(L"CreateTorrentCanceling"));
	}

	void CreateTorrentProgressWindow::CloseButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		Close();
	}

	void CreateTorrentProgressWindow::OpenFolderButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		ShellExecuteW(nullptr, L"open", m_targetPath.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}
}
