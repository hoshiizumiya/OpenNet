#include "pch.h"
#include "TorrentCheckGeneralPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.cpp")
#include "UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.cpp"
#endif

#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include "Helpers/WindowHelper.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TorrentCheckGeneralPage::TorrentCheckGeneralPage()
	{
		InitializeComponent();
	}

	winrt::Windows::Foundation::IAsyncAction TorrentCheckGeneralPage::TorrentCheckGeneralPageFolderPicker_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{

		auto lifetime = get_strong();

		auto button = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::Button>();
		if (!button)
		{
			co_return;
		}

		button.IsEnabled(false);

		try
		{
			auto folderPath = co_await PickFolderAsync();
			if (folderPath.empty())
			{
				button.IsEnabled(true);
				co_return;
			}

			auto box = TorrentCheckGeneralPageSavePath();
			if (!box)
			{
				button.IsEnabled(true);
				co_return;
			}

			auto dq = DispatcherQueue();
			if (dq)
			{
				dq.TryEnqueue([box, folderPath]()
							  {
								  box.Text(folderPath);
							  });
			}

		}
		catch (winrt::hresult_error const&)
		{

		}

		button.IsEnabled(true);
	}

	Windows::Foundation::IAsyncOperation<hstring> TorrentCheckGeneralPage::PickFolderAsync()
	{

		if (auto appWindow = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetAppWindowForElement(*this))
		{
			// update to WASdk https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.windows.storage.pickers#remarks
			Microsoft::Windows::Storage::Pickers::FolderPicker folderPicker(appWindow.Id());
			folderPicker.SuggestedStartLocation(Microsoft::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
			folderPicker.ViewMode(Microsoft::Windows::Storage::Pickers::PickerViewMode::List);

			// Show the picker dialog window
			auto&& folderRef = co_await folderPicker.PickSingleFolderAsync();
			if (folderRef)
			{
				auto path{ folderRef.Path() };
#ifdef _DEBUG
				OutputDebugString(L"Pick path success");
#endif
				co_return path;
			}
		}
		else
		{
			// error handling logic
		}
		co_return hstring{};
	}
}
