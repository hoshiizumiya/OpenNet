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

		auto tag = winrt::unbox_value_or<winrt::hstring>(checkBox.Tag(), L"");
		if (!tag.empty())
		{
			m_viewModel.SelectByExtension(tag);
			if (auto selectedSizeText = SelectedSizeText())
			{
				selectedSizeText.Text(m_viewModel.SelectedSize());
			}
		}
	}

	void TorrentCheckGeneralPage::FilterCheckBox_Unchecked(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		// When a filter is unchecked, deselect files matching that filter
		ApplyFilters();
	}

	void TorrentCheckGeneralPage::TorrentFileTreeView_SelectionChanged(
		winrt::Microsoft::UI::Xaml::Controls::TreeView const& sender,
		winrt::Microsoft::UI::Xaml::Controls::TreeViewSelectionChangedEventArgs const& args)
	{
		// Sync TreeView selection with ViewModel IsSelected property
		if (!m_viewModel) return;

		// Handle added items
		for (auto const& item : args.AddedItems())
		{
			if (auto fileInfo = item.try_as<winrt::OpenNet::ViewModels::TorrentFileInfoViewModel>())
			{
				fileInfo.IsSelected(true);
			}
		}

		// Handle removed items
		for (auto const& item : args.RemovedItems())
		{
			if (auto fileInfo = item.try_as<winrt::OpenNet::ViewModels::TorrentFileInfoViewModel>())
			{
				fileInfo.IsSelected(false);
			}
		}

		m_viewModel.RefreshSelectedSize();
		UpdateSelectedSizeDisplay();
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

	void TorrentCheckGeneralPage::ApplyFilters()
	{
		if (!m_viewModel) return;

		// Get filter states
		bool selectVideo = VideoFilterCheckBox() && VideoFilterCheckBox().IsChecked().Value();
		bool selectAudio = AudioFilterCheckBox() && AudioFilterCheckBox().IsChecked().Value();
		bool selectPicture = PictureFilterCheckBox() && PictureFilterCheckBox().IsChecked().Value();
		bool selectOther = OtherFilterCheckBox() && OtherFilterCheckBox().IsChecked().Value();

		// If no filters are selected, deselect all
		bool anyFilterSelected = selectVideo || selectAudio || selectPicture || selectOther;

		auto files = m_viewModel.Files();
		for (uint32_t i = 0; i < files.Size(); ++i)
		{
			auto file = files.GetAt(i);
			std::wstring fileName{ file.FileName() };

			// Get file extension (lowercase)
			std::wstring ext;
			auto dotPos = fileName.rfind(L'.');
			if (dotPos != std::wstring::npos)
			{
				ext = fileName.substr(dotPos);
				std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
			}

			bool shouldSelect = false;

			if (anyFilterSelected)
			{
				// Check if matches video extensions
				if (selectVideo)
				{
					std::wstring videoExts(VideoExtensions);
					if (videoExts.find(ext) != std::wstring::npos)
					{
						shouldSelect = true;
					}
				}

				// Check if matches audio extensions
				if (!shouldSelect && selectAudio)
				{
					std::wstring audioExts(AudioExtensions);
					if (audioExts.find(ext) != std::wstring::npos)
					{
						shouldSelect = true;
					}
				}

				// Check if matches picture extensions
				if (!shouldSelect && selectPicture)
				{
					std::wstring pictureExts(PictureExtensions);
					if (pictureExts.find(ext) != std::wstring::npos)
					{
						shouldSelect = true;
					}
				}

				// Check if "Other" (not matching any known category)
				if (!shouldSelect && selectOther)
				{
					std::wstring videoExts(VideoExtensions);
					std::wstring audioExts(AudioExtensions);
					std::wstring pictureExts(PictureExtensions);

					bool isKnownType = 
						videoExts.find(ext) != std::wstring::npos ||
						audioExts.find(ext) != std::wstring::npos ||
						pictureExts.find(ext) != std::wstring::npos;

					if (!isKnownType && !ext.empty())
					{
						shouldSelect = true;
					}
				}
			}

			file.IsSelected(shouldSelect);
		}

		m_viewModel.RefreshSelectedSize();
		UpdateSelectedSizeDisplay();
	}

	void TorrentCheckGeneralPage::UpdateSelectedSizeDisplay()
	{
		if (auto selectedSizeText = SelectedSizeText())
		{
			if (m_viewModel)
			{
				selectedSizeText.Text(m_viewModel.SelectedSize());
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
			// Get drive root from path (e.g., "C:" from "C:\Users\...")
			std::wstring path{ savePath };
			if (path.empty()) return;

			// Ensure path ends with backslash for GetDiskFreeSpaceExW
			if (path.back() != L'\\' && path.back() != L'/')
			{
				path += L'\\';
			}

			ULARGE_INTEGER freeBytesAvailable{};
			ULARGE_INTEGER totalBytes{};
			ULARGE_INTEGER totalFreeBytes{};

			if (GetDiskFreeSpaceExW(
				path.c_str(),
				&freeBytesAvailable,
				&totalBytes,
				&totalFreeBytes))
			{
				// Format the sizes
				std::wstring requiredStr = L"Required: " + FormatBytes(m_viewModel.SelectedSize().size());
				std::wstring freeStr = L"Free: " + FormatBytes(totalFreeBytes.QuadPart);

				// Update UI
				if (auto diskSpaceText = DiskSpaceText())
				{
					diskSpaceText.Text(winrt::hstring(requiredStr));
				}
				if (auto freeSpaceText = FreeSpaceText())
				{
					freeSpaceText.Text(winrt::hstring(freeStr));
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
