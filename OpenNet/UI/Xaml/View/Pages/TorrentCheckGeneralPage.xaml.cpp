#include "pch.h"
#include "TorrentCheckGeneralPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.cpp")
#include "UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.cpp"
#endif

#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <shlobj.h>
#include <wil/resource.h>
#include "Helpers/WindowHelper.h"
#include <algorithm>
#include <cctype>
#include <windows.h>
#include <iomanip>
#include <sstream>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TorrentCheckGeneralPage::TorrentCheckGeneralPage()
	{
		InitializeComponent();
	}

	void TorrentCheckGeneralPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		// Receive the ViewModel from navigation parameter
		auto param = e.Parameter();
		if (param)
		{
			m_viewModel = param.try_as<winrt::OpenNet::ViewModels::TorrentMetadataViewModel>();
			if (m_viewModel)
			{
				// Update UI bindings
				if (auto nameText = TorrentNameText())
				{
					nameText.Text(m_viewModel.TorrentName());
				}
				if (auto sizeText = TorrentSizeText())
				{
					sizeText.Text(m_viewModel.TotalSize());
				}
				if (auto selectedSizeText = SelectedSizeText())
				{
					selectedSizeText.Text(m_viewModel.SelectedSize());
				}
				if (auto savePathBox = TorrentCheckGeneralPageSavePath())
				{
					if (m_viewModel.SavePath().empty())
					{
						// Set default save path
						wil::unique_cotaskmem_string downloadsPath;
						if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloadsPath)))
						{
							std::wstring defaultPath(downloadsPath.get());

							savePathBox.Text(winrt::hstring(defaultPath));
							m_viewModel.SavePath(winrt::hstring(defaultPath));

							// Update disk space display
							UpdateDiskSpaceDisplay(winrt::hstring(defaultPath));
						}
					}
					else
					{
						savePathBox.Text(m_viewModel.SavePath());

						// Update disk space display
						UpdateDiskSpaceDisplay(m_viewModel.SavePath());
					}
				}
				if (auto fileNameBox = TorrentFileNameTextBox())
				{
					fileNameBox.Text(m_viewModel.TorrentName());
				}

				// NOTE: TreeView ItemsSource binding is done in XAML, no need to set it here
			}
		}
	}

	winrt::Windows::Foundation::IAsyncAction TorrentCheckGeneralPage::TorrentCheckGeneralPageFolderPicker_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
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
			if (box)
			{
				box.Text(folderPath);
			}

			if (m_viewModel)
			{
				m_viewModel.SavePath(folderPath);
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
			Microsoft::Windows::Storage::Pickers::FolderPicker folderPicker(appWindow.Id());
			folderPicker.SuggestedStartLocation(Microsoft::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
			folderPicker.ViewMode(Microsoft::Windows::Storage::Pickers::PickerViewMode::List);

			auto&& folderRef = co_await folderPicker.PickSingleFolderAsync();
			if (folderRef)
			{
				auto path{ folderRef.Path() };
				co_return path;
			}
		}
		co_return hstring{};
	}

	void TorrentCheckGeneralPage::SelectAllCheckBox_Checked(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel)
		{
			m_viewModel.SelectAll();
			UpdateSelectedSizeDisplay();
		}
	}

	void TorrentCheckGeneralPage::SelectAllCheckBox_Unchecked(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel)
		{
			m_viewModel.DeselectAll();
			UpdateSelectedSizeDisplay();
		}
	}

	void TorrentCheckGeneralPage::FilterCheckBox_Checked(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto checkBox = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::CheckBox>();
		if (!checkBox || !m_viewModel)
		{
			return;
		}

		// Use ApplyFilters to correctly combine all checked filter categories
		ApplyFilters();
	}

	void TorrentCheckGeneralPage::FilterCheckBox_Unchecked(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		// When a filter is unchecked, deselect files matching that filter
		ApplyFilters();
	}

	void TorrentCheckGeneralPage::TorrentFileTreeView_SelectionChanged(
		winrt::Microsoft::UI::Xaml::Controls::TreeView const&,
		winrt::Microsoft::UI::Xaml::Controls::TreeViewSelectionChangedEventArgs const&)
	{
		// Selection is now handled by individual checkboxes, not TreeView selection
		// This handler can be left empty or removed from the header
	}

	void TorrentCheckGeneralPage::FileCheckBox_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		// Update selected size when individual file checkbox is clicked
		if (m_viewModel)
		{
			m_viewModel.RefreshSelectedSize();
			UpdateSelectedSizeDisplay();
		}
	}

	// Helper function to check if a filename ends with any extension in a comma-separated list
	static bool MatchesExtensionList(const std::wstring& fileName, std::wstring_view extList)
	{
		if (fileName.empty()) return false;

		// Parse comma-separated extensions
		std::wstring list{ extList };
		std::wstring::size_type start = 0;
		std::wstring::size_type end = 0;

		while ((end = list.find(L',', start)) != std::wstring::npos || start < list.length())
		{
			if (end == std::wstring::npos)
				end = list.length();

			std::wstring ext = list.substr(start, end - start);

			// Trim whitespace
			while (!ext.empty() && ext[0] == L' ') ext.erase(0, 1);
			while (!ext.empty() && ext.back() == L' ') ext.pop_back();

			// Check if filename ends with this extension
			if (!ext.empty() && fileName.length() >= ext.length())
			{
				if (fileName.substr(fileName.length() - ext.length()) == ext)
					return true;
			}

			start = end + 1;
			if (start >= list.length())
				break;
		}
		return false;
	}

	void TorrentCheckGeneralPage::ApplyFilters()
	{
		if (!m_viewModel) return;

		// Get filter states
		bool selectVideo = VideoFilterCheckBox() && VideoFilterCheckBox().IsChecked().Value();
		bool selectAudio = AudioFilterCheckBox() && AudioFilterCheckBox().IsChecked().Value();
		bool selectPicture = PictureFilterCheckBox() && PictureFilterCheckBox().IsChecked().Value();
		bool selectOther = OtherFilterCheckBox() && OtherFilterCheckBox().IsChecked().Value();

		// Build extension list based on selected filters
		std::wstring extensionList;

		if (selectVideo)
		{
			if (!extensionList.empty()) extensionList += L",";
			extensionList += VideoExtensions;
		}
		if (selectAudio)
		{
			if (!extensionList.empty()) extensionList += L",";
			extensionList += AudioExtensions;
		}
		if (selectPicture)
		{
			if (!extensionList.empty()) extensionList += L",";
			extensionList += PictureExtensions;
		}

		// If no filters selected, deselect all
		if (!selectVideo && !selectAudio && !selectPicture && !selectOther)
		{
			m_viewModel.DeselectAll();
		}
		// If "Other" is the only one selected, we need special handling
		else if (selectOther && !selectVideo && !selectAudio && !selectPicture)
		{
			// Select files that don't match known categories
			auto files = m_viewModel.Files();
			for (uint32_t i = 0; i < files.Size(); ++i)
			{
				auto file = files.GetAt(i);
				std::wstring fileName{ file.FileName() };
				std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::towlower);

				bool isKnownType = 
					MatchesExtensionList(fileName, VideoExtensions) ||
					MatchesExtensionList(fileName, AudioExtensions) ||
					MatchesExtensionList(fileName, PictureExtensions);

				file.IsSelected(!isKnownType);
			}
			m_viewModel.RefreshSelectedSize();
		}
		else if (!extensionList.empty())
		{
			// If selectOther is also checked, we need to include files not matching known types
			if (selectOther)
			{
				// Complex case: selected categories + other
				auto files = m_viewModel.Files();
				for (uint32_t i = 0; i < files.Size(); ++i)
				{
					auto file = files.GetAt(i);
					std::wstring fileName{ file.FileName() };
					std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::towlower);

					bool matchesSelected = false;
					if (selectVideo) matchesSelected = matchesSelected || MatchesExtensionList(fileName, VideoExtensions);
					if (selectAudio) matchesSelected = matchesSelected || MatchesExtensionList(fileName, AudioExtensions);
					if (selectPicture) matchesSelected = matchesSelected || MatchesExtensionList(fileName, PictureExtensions);

					bool isKnownType = 
						MatchesExtensionList(fileName, VideoExtensions) ||
						MatchesExtensionList(fileName, AudioExtensions) ||
						MatchesExtensionList(fileName, PictureExtensions);

					// Select if matches selected category OR is "other" (unknown type)
					file.IsSelected(matchesSelected || !isKnownType);
				}
				m_viewModel.RefreshSelectedSize();
			}
			else
			{
				// Simple case: just selected categories
				m_viewModel.SelectByExtension(winrt::hstring(extensionList));
			}
		}

		UpdateSelectedSizeDisplay();
	}

	void TorrentCheckGeneralPage::UpdateSelectedSizeDisplay()
	{
		if (auto selectedSizeText = SelectedSizeText())
		{
			if (m_viewModel)
			{
				selectedSizeText.Text(m_viewModel.SelectedSize());
				// Also update disk space display when selection changes
				if (auto savePathBox = TorrentCheckGeneralPageSavePath())
				{
					UpdateDiskSpaceDisplay(savePathBox.Text());
				}
			}
		}
	}

	std::wstring FormatBytes(ULONGLONG bytes)
	{
		const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
		double size = static_cast<double>(bytes);
		int unit = 0;

		while (size >= 1024.0 && unit < 4)
		{
			size /= 1024.0;
			unit++;
		}

		std::wostringstream stream;
		stream << std::fixed << std::setprecision(2) << size << L" " << units[unit];
		return stream.str();
	}

	void TorrentCheckGeneralPage::UpdateDiskSpaceDisplay(const hstring& savePath)
	{
		try
		{
			// Get drive root from path (e.g., "C:\" from "C:\Users\...")
			std::wstring path{ savePath };
			if (path.empty()) return;

			// Extract drive root (e.g., "C:\")
			std::wstring drivePath;
			if (path.length() >= 2 && path[1] == L':')
			{
				// Standard drive letter path (e.g., "C:\Users\...")
				drivePath = path.substr(0, 2) + L"\\";
			}
			else if (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\')
			{
				// UNC path (e.g., "\\server\share") - use the path as is
				drivePath = path;
				if (drivePath.back() != L'\\')
				{
					drivePath += L'\\';
				}
			}
			else
			{
				// Relative path or other format - try to use the path directly
				drivePath = path;
				if (!drivePath.empty() && drivePath.back() != L'\\')
				{
					drivePath += L'\\';
				}
			}

			ULARGE_INTEGER freeBytesAvailable{};
			ULARGE_INTEGER totalBytes{};
			ULARGE_INTEGER totalFreeBytes{};

			if (GetDiskFreeSpaceExW(
				drivePath.c_str(),
				&freeBytesAvailable,
				&totalBytes,
				&totalFreeBytes))
			{
				m_totalDiskBytes = totalBytes.QuadPart;
				m_freeDiskBytes = totalFreeBytes.QuadPart;

				// Get required bytes from ViewModel
				int64_t requiredBytes = m_viewModel ? m_viewModel.SelectedSizeBytes() : 0;

				// Format the sizes
				std::wstring requiredStr = FormatBytes(static_cast<ULONGLONG>(requiredBytes));
				std::wstring freeStr = FormatBytes(totalFreeBytes.QuadPart);
				std::wstring totalStr = FormatBytes(totalBytes.QuadPart);

				// Update text displays
				if (auto diskSpaceText = DiskSpaceText())
				{
					diskSpaceText.Text(winrt::hstring(requiredStr));
				}
				if (auto freeSpaceText = FreeSpaceText())
				{
					freeSpaceText.Text(winrt::hstring(freeStr));
				}
				if (auto totalSpaceText = TotalSpaceText())
				{
					totalSpaceText.Text(winrt::hstring(totalStr));
				}

				// Update progress bar - show used space as percentage of total
				if (auto progressBar = DiskSpaceProgressBar())
				{
					// Calculate usage percentage (used / total)
					uint64_t usedBytes = totalBytes.QuadPart - totalFreeBytes.QuadPart;
					double usagePercent = (static_cast<double>(usedBytes) / static_cast<double>(totalBytes.QuadPart)) * 100.0;
					progressBar.Value(usagePercent);

					// Check if there's enough space for the required download
					bool hasEnoughSpace = static_cast<uint64_t>(requiredBytes) <= totalFreeBytes.QuadPart;
					progressBar.ShowError(!hasEnoughSpace);
					progressBar.ShowPaused(false);  // Don't show paused state, just error if not enough space
				}
			}
		}
		catch (...)
		{
			// Silently fail if unable to get disk info
		}
	}

	void TorrentCheckGeneralPage::TorrentCheckGeneralPageSavePath_TextChanged(
		winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender,
		winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args)
	{
		std::wstring text{ sender.Text() };
		if (!text.empty())
		{
			UpdateDiskSpaceDisplay(sender.Text());
			if (m_viewModel)
			{
				m_viewModel.SavePath(sender.Text());
			}
		}
	}
}
