#include "pch.h"
#include "ThemesSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp"
#endif

#include "Core/AppSettingsDatabase.h"
#include "Helpers/WindowHelper.h"
#include <Shlwapi.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/WinUI3Package.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Microsoft::UI::Composition::SystemBackdrops;

namespace
{
	winrt::Microsoft::UI::Xaml::Media::Stretch StretchFromIndex(int index)
	{
		switch (index)
		{
			case 1: return winrt::Microsoft::UI::Xaml::Media::Stretch::Fill;
			case 2: return winrt::Microsoft::UI::Xaml::Media::Stretch::Uniform;
			case 3: return winrt::Microsoft::UI::Xaml::Media::Stretch::UniformToFill;
			case 0:
			default:
				return winrt::Microsoft::UI::Xaml::Media::Stretch::None;
		}
	}
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	ThemesSettingsPage::ThemesSettingsPage()
	{
		Loaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			LoadBackdropSettings();
		});
	}

	void ThemesSettingsPage::LoadBackdropSettings()
	{
		m_isInitializing = true;

		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto const backgroundType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_type", 1)), 0, 2);
		auto const micaType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "mica_type", 1)), 0, 1);
		auto const acrylicType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "acrylic_type", 0)), 0, 2);
		auto const imageStretch = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_stretch", 3)), 0, 3);
		auto const imageOpacity = std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_opacity").value_or(20.0), 0.0, 100.0);

		BackgroundComboBox().SelectedIndex(backgroundType);
		MicaTypeComboBox().SelectedIndex(micaType);
		AcrylicTypeComboBox().SelectedIndex(acrylicType);
		ImageStretchComboBox().SelectedIndex(imageStretch);
		ImageOpacitySlider().Value(imageOpacity);

		auto imagePath = db.GetStringW(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_image").value_or(L"");
		ImagePathText().Text(imagePath);

		SyncSelectionWithCurrentBackdrop();
		UpdateBackdropCardState();
		m_isInitializing = false;
	}

	void ThemesSettingsPage::SyncSelectionWithCurrentBackdrop()
	{
		auto window = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowForElement(*this);
		if (!window)
		{
			return;
		}

		auto const backdrop = window.SystemBackdrop();
		if (!backdrop)
		{
			BackgroundComboBox().SelectedIndex(0);
			return;
		}

		if (auto mica = backdrop.try_as<MicaBackdrop>())
		{
			BackgroundComboBox().SelectedIndex(1);
			MicaTypeComboBox().SelectedIndex(mica.Kind() == MicaKind::BaseAlt ? 1 : 0);
			return;
		}

		if (auto acrylic = backdrop.try_as<winrt::WinUI3Package::CustomAcrylicBackdrop>())
		{
			BackgroundComboBox().SelectedIndex(2);
			switch (acrylic.Kind())
			{
				case DesktopAcrylicKind::Base:
					AcrylicTypeComboBox().SelectedIndex(1);
					break;
				case DesktopAcrylicKind::Thin:
					AcrylicTypeComboBox().SelectedIndex(2);
					break;
				case DesktopAcrylicKind::Default:
				default:
					AcrylicTypeComboBox().SelectedIndex(0);
					break;
			}
		}
	}

	void ThemesSettingsPage::UpdateBackdropCardState()
	{
		auto const backgroundType = static_cast<int>(BackgroundComboBox().SelectedIndex());
		AcrylicTypeCard().IsEnabled(backgroundType == 2);
		MicaTypeCard().IsEnabled(backgroundType == 1);
	}

	void ThemesSettingsPage::ApplyBackdropFromSelection()
	{
		auto window = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowForElement(*this);
		if (!window)
		{
			return;
		}

		auto const backgroundType = static_cast<int>(BackgroundComboBox().SelectedIndex());
		switch (backgroundType)
		{
			case 1:
			{
				if (!MicaController::IsSupported())
				{
					window.SystemBackdrop(nullptr);
					break;
				}

				auto mica = MicaBackdrop{};
				auto const micaType = static_cast<int>(MicaTypeComboBox().SelectedIndex());
				mica.Kind(micaType == 1 ? MicaKind::BaseAlt : MicaKind::Base);
				window.SystemBackdrop(mica);
				break;
			}
			case 2:
			{
				if (!DesktopAcrylicController::IsSupported())
				{
					window.SystemBackdrop(nullptr);
					break;
				}

				auto acrylic = winrt::WinUI3Package::CustomAcrylicBackdrop{};
				auto const acrylicType = static_cast<int>(AcrylicTypeComboBox().SelectedIndex());
				switch (acrylicType)
				{
					case 1:
						acrylic.Kind(DesktopAcrylicKind::Base);
						break;
					case 2:
						acrylic.Kind(DesktopAcrylicKind::Thin);
						break;
					case 0:
					default:
						acrylic.Kind(DesktopAcrylicKind::Default);
						break;
				}

				window.SystemBackdrop(acrylic);
				break;
			}
			case 0:
			default:
				window.SystemBackdrop(nullptr);
				break;
		}
	}

	void ThemesSettingsPage::ApplyImageBackgroundFromSettings()
	{
		auto window = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowForElement(*this);
		if (!window)
		{
			return;
		}

		auto panel = window.Content().try_as<winrt::Microsoft::UI::Xaml::Controls::Panel>();
		if (!panel)
		{
			return;
		}

		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto imagePath = db.GetStringW(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_image").value_or(L"");
		if (imagePath.empty())
		{
			panel.Background(nullptr);
			return;
		}

		constexpr DWORD kMaxUrlLen = 2083;
		WCHAR encodedUrl[kMaxUrlLen]{};
		DWORD urlLen = kMaxUrlLen;
		std::wstring imageUri;
		if (SUCCEEDED(UrlCreateFromPathW(imagePath.c_str(), encodedUrl, &urlLen, 0)))
		{
			imageUri = encodedUrl;
		}
		else
		{
			imageUri = L"file:///" + imagePath;
			std::replace(imageUri.begin(), imageUri.end(), L'\\', L'/');
		}

		auto stretchIndex = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_stretch", 3)), 0, 3);
		auto opacity = std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_opacity").value_or(20.0), 0.0, 100.0) / 100.0;

		try
		{
			auto bitmap = BitmapImage{};
			bitmap.UriSource(winrt::Windows::Foundation::Uri{ imageUri });

			auto brush = ImageBrush{};
			brush.ImageSource(bitmap);
			brush.Stretch(StretchFromIndex(stretchIndex));
			brush.Opacity(opacity);
			panel.Background(brush);
		}
		catch (...)
		{
		}
	}

	void ThemesSettingsPage::BackgroundComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;

		auto const selectedIndex = static_cast<int>(BackgroundComboBox().SelectedIndex());
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_type", selectedIndex);

		UpdateBackdropCardState();
		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::MicaTypeComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;

		auto const selectedIndex = static_cast<int>(MicaTypeComboBox().SelectedIndex());
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "mica_type", selectedIndex);

		ApplyBackdropFromSelection();
	}

	void ThemesSettingsPage::AcrylicTypeComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;

		auto const selectedIndex = static_cast<int>(AcrylicTypeComboBox().SelectedIndex());
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "acrylic_type", selectedIndex);

		ApplyBackdropFromSelection();
	}

	winrt::Windows::Foundation::IAsyncAction ThemesSettingsPage::SetImageButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto picker = winrt::Microsoft::Windows::Storage::Pickers::FileOpenPicker(this->XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.SuggestedStartLocation(winrt::Microsoft::Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);
		picker.FileTypeFilter().Append(L".png");
		picker.FileTypeFilter().Append(L".jpg");
		picker.FileTypeFilter().Append(L".bmp");
		picker.FileTypeFilter().Append(L".jpeg");

		auto result = co_await picker.PickSingleFileAsync();
		std::wstring_view result_path;
		if (!result)
		{
			co_return;
		}
		else
		{
			result_path = result.Path();
		}

		try
		{
			auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(result_path);

			if (file && file.IsAvailable() && (file.FileType() == L".png" || file.FileType() == L".jpg" || file.FileType() == L".bmp" || file.FileType() == L".jpeg"))
			{
				auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
				db.SetStringW(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_image", file.Path());
				ImagePathText().Text(result_path);
				ApplyImageBackgroundFromSettings();
			}
		}
		catch (hresult_error)
		{
			co_return;
		}
	}

	void ThemesSettingsPage::ClearImageButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		db.SetStringW(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_image", L"");
		ImagePathText().Text(L"");
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ImageStretchComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			"image_stretch",
			static_cast<int>(ImageStretchComboBox().SelectedIndex()));
		ApplyImageBackgroundFromSettings();
	}

	void ThemesSettingsPage::ImageOpacitySlider_ValueChanged(IInspectable const&, RangeBaseValueChangedEventArgs const&)
	{
		if (m_isInitializing) return;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetDouble(
			::OpenNet::Core::AppSettingsDatabase::CAT_UI,
			"image_opacity",
			ImageOpacitySlider().Value());
		ApplyImageBackgroundFromSettings();
	}
}
