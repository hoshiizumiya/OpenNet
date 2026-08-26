#pragma once

import winrt.XamlToolkit.Labs.WinUI;
import OpenNet.ViewModels.ObservableMixin;
import winrt.OpenNet.UI.Xaml.Control.Progress.Storage;
#include "UI/Xaml/View/Pages/TaskFilesPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskFilesPage : TaskFilesPageT<TaskFilesPage>, ::OpenNet::ViewModels::ObservableMixin<TaskFilesPage>
	{
		TaskFilesPage();
		~TaskFilesPage();
		void InitializeComponent();

		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

		// ComboBox selection changed for file priority
		void FilePriority_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
		void DataTable_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void ColumnHeader_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ColumnHeader_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
		void ColumnMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void AutoSizeSelectedColumn_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void AutoSizeAllColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ColumnVisibility_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ResetColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void FileDataRow_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void FilesListView_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
		void FilesListView_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& args);
		void FileContextMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void OpenFile_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void PlayFile_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OpenFileLocation_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction RenameFile_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_vmPropertyChangedToken{};

		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerTickToken{};
		std::atomic_bool m_isActive{};
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_fileItems{ nullptr };

		// Suppress priority change events during list refresh
		bool m_isRefreshing{ false };
		winrt::hstring m_sortColumn;
		int m_sortDirection{}; // 0=none, 1=ascending, 2=descending
		winrt::XamlToolkit::Labs::WinUI::DataColumn m_contextColumn{ nullptr };

		void Unsubscribe();
		void StopRefreshTimer() noexcept;
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		void RefreshFileList();
		void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void UpdateSortHeaders();
		void AutoSizeColumn(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column);
		winrt::XamlToolkit::Labs::WinUI::DataColumn ColumnForTag(winrt::hstring const& tag);
		void SynchronizeFileRows();

		struct SelectedFileContext
		{
			std::string taskId;
			int fileIndex{};
			std::filesystem::path relativePath;
			std::filesystem::path fullPath;
		};
		std::optional<SelectedFileContext> GetSelectedFileContext();
		bool LaunchSelectedFile(wchar_t const* verb);
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskFilesPage : TaskFilesPageT<TaskFilesPage, implementation::TaskFilesPage>
	{
	};
}
