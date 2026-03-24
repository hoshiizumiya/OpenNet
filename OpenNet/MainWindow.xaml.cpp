#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "UI/Xaml/View/Pages/MainView.xaml.h"
#include "Helpers/WindowHelper.h"
#include "Core/AppSettingsDatabase.h"
#include "winrt/microsoft.ui.interop.h"
#include <Shlwapi.h>
#include <wil/resource.h>
#include <resource.h>
#include <windows.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/WinUI3Package.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace ::OpenNet::Helpers::WinUIWindowHelper;

namespace winrt::OpenNet::implementation
{
	MainWindow::MainWindow()
	{
		InitializeComponent();
		SetTitleBar(AppTitleBar());
		InitWindowStyle(*this);

		AppWindow().SetIcon(L"Assets/AppIcons/win3264.ico");

		// Listen for back-button state changes from MainView
		MainContentView().CanGoBackChanged([this](IInspectable const&, bool canGoBack)
		{
			AppTitleBar().IsBackButtonVisible(canGoBack);
		});

		Closed([this](auto&&, auto&&)
		{
			PlacementRestoration::Save(*this);

			// Stop ViewModel background thread (speed refresh)
			try
			{
				if (auto vm = ViewModel())
					vm.Shutdown();
			}
			catch (...) { OutputDebugStringA("MainWindow: ViewModel shutdown error\n"); }
		});
	}

	winrt::OpenNet::ViewModels::MainViewModel MainWindow::ViewModel()
	{
		return MainContentView().ViewModel();
	}

	void MainWindow::Navigate(hstring const& tag)
	{
		MainContentView().Navigate(tag);
	}

	void MainWindow::InitWindowStyle(Window const& window)
	{
		window.ExtendsContentIntoTitleBar(true);
		if (auto appWindow = window.AppWindow())
		{
			appWindow.TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Tall);
			PlacementRestoration::Enable(*this);
#ifdef _DEBUG
			{
				AppTitleBar().Subtitle(L"Dev");
			}
#endif
		}
	}

	void MainWindow::AppTitleBar_BackRequested(Microsoft::UI::Xaml::Controls::TitleBar const&, IInspectable const&)
	{
		MainContentView().GoBack();
	}

	void MainWindow::Grid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);

		if (auto rootGrid = sender.try_as<FrameworkElement>())
		{
			if (auto xamlRoot = rootGrid.XamlRoot())
			{
				xamlRoot.Changed({ this, &MainWindow::RootGridXamlRoot_Changed });
			}
		}
	}

	IAsyncAction MainWindow::LoadBackground()
	{
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto imagePath = db.GetStringW(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_image").value_or(L"");
		if (imagePath.empty())
		{
			RootGrid().Background(nullptr);
			co_return;
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

		auto const stretchIndex = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_stretch", 3)), 0, 3);
		auto const opacity = std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_opacity").value_or(20.0), 0.0, 100.0) / 100.0;

		auto brush = ImageBrush{};
		auto bitmap = BitmapImage{};
		bitmap.UriSource(winrt::Windows::Foundation::Uri{ imageUri });
		brush.ImageSource(bitmap);
		switch (stretchIndex)
		{
		case 1:
			brush.Stretch(Stretch::Fill);
			break;
		case 2:
			brush.Stretch(Stretch::Uniform);
			break;
		case 3:
			brush.Stretch(Stretch::UniformToFill);
			break;
		case 0:
		default:
			brush.Stretch(Stretch::None);
			break;
		}
		brush.Opacity(opacity);
		RootGrid().Background(brush);
		co_return;
	}

	void MainWindow::RootGridXamlRoot_Changed(XamlRoot /*sender*/, XamlRootChangedEventArgs /*args*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);
	}
}
