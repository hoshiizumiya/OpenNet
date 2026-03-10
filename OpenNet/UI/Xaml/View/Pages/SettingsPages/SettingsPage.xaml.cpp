#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/SettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/SettingsPage.g.cpp"
#endif

#include <winrt/Windows.System.h>
#include "Folder.h"
#include "MainSettingsPage.xaml.h"
#include "../../MainWindow.xaml.h"
#include "../../Helpers/WindowHelper.h"
#include "../../Helpers/ThemeHelper.h"
#include "winrt/Microsoft.Windows.AppLifecycle.h"
// 解析 BitmapImage 的构造函数实现
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Dispatching.h> 
#include <wil/cppwinrt_helpers.h>

// 用于解析字符串为整数
#include <sstream>
#include <winrt/Microsoft.Windows.Storage.h>
#include <Shlwapi.h>   // UrlCreateFromPathW

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Microsoft::UI::Xaml::Media::Animation;
using namespace ::winrt::Windows::Globalization;
using namespace Microsoft::Windows::ApplicationModel::Resources;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	// 正确定义类静态成员（不要使用文件作用域的 static 关键字）
	SettingsPage* SettingsPage::s_current = nullptr;

	SettingsPage::SettingsPage()
	{
		InitializeComponent();
		s_current = this;
		m_dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

		// Defer all UI element initialization to Loaded event per C++/WinRT guidelines
		Loaded([this](IInspectable const& sender, RoutedEventArgs const& e)
		{
			m_loadAction = OnSettingsPageLoadedAsync(sender, e);
		});

	}
	SettingsPage::~SettingsPage()
	{
		if (m_loadAction)
		{
			m_loadAction.Cancel();
			m_loadAction = nullptr;
		}
		if (s_current == this)
		{
			s_current = nullptr;
		}
	}

	void SettingsPage::SoftLanguageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*args*/)
	{
		auto combo = sender.try_as<ComboBox>();
		if (!combo) return;

		auto idx = combo.SelectedIndex();
		if (idx < 0) return;

		// Read the ComboBoxItem.Tag as the language tag (we stored it as boxed hstring)
		winrt::hstring target;
		auto sel = combo.SelectedItem().try_as<ComboBoxItem>();
		if (sel)
		{
			auto boxed = sel.Tag();
			if (boxed)
			{
				// Tag was stored as boxed hstring
				target = unbox_value_or<hstring>(boxed, hstring(L""));
			}
		}

		// Show/Hide restart banner depending on if it differs from initial
		m_pendingLanguageOverride = target;
		m_hasPendingLangChange = (m_pendingLanguageOverride != m_initialLanguageOverride);
		RestartLanguageInfoBar().IsOpen(m_hasPendingLangChange);

		// Map empty tag to auto behavior (follow system)
		if (target.empty())
		{
			// Apply override with empty string to follow system
			ApplicationLanguages::PrimaryLanguageOverride(L"");
			return;
		}

		// If already set, no-op
		if (ApplicationLanguages::PrimaryLanguageOverride() == target)
		{
			return;
		}

		// Apply language override
		ApplicationLanguages::PrimaryLanguageOverride(target);

		// Don't restart automatically; let user choose via InfoBar, there may cause task stop or data loss...
		// Recreate main window to reload XAML resources with new language
		// This way is not work, because the app must close.
		//auto oldWindow = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowForElement(*this);
		//auto newWindow = winrt::make<winrt::OpenNet::implementation::MainWindow>();
		//::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(newWindow);
		//oldWindow.Close();
		//newWindow.Activate();
		//winrt::Microsoft::Windows::AppLifecycle::AppInstance::Restart(L"/safemode");

		//Future : use json/xml to define language option to aviod can not change back.
	}

	void SettingsPage::RestartToApplyLanguage_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		winrt::Microsoft::Windows::AppLifecycle::AppInstance::Restart(L"");
	}

	::winrt::Windows::Foundation::IAsyncAction SettingsPage::GithubDefaultLaunch()
	{
		// Launch the URI.
		if (co_await ::winrt::Windows::System::Launcher::LaunchUriAsync(m_githubReleaseLinkUri))
		{
			// URI launched.
		}
		else
		{
#ifdef DEBUG
			OutputDebugStringW(L"Fail to launch GitHub Release Page\n");
#endif

		}
	}

	SettingsPage* SettingsPage::Current()
	{
		return s_current;
	}

	void SettingsPage::SoftBackgroundCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
	{
		// No-op placeholder
	}

	void SettingsPage::StartPageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
	{
		if (m_isStartPageLoading) return;

		auto combo = StartPageCombobox();
		if (combo.SelectedIndex() < 0) return;

		auto sel = combo.SelectedItem().try_as<ComboBoxItem>();
		if (!sel) return;

		auto tag = unbox_value_or<hstring>(sel.Tag(), L"");

		try
		{
			auto localSettings = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings();
			localSettings.Values().Insert(L"StartPage", box_value(tag));
		}
		catch (...) {}
	}

	void SettingsPage::themeMode_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
	{
		// Read selected theme from ComboBoxItem.Tag
		hstring tag;
		if (auto combo = sender.try_as<ComboBox>())
		{
			if (auto sel = combo.SelectedItem().try_as<ComboBoxItem>())
			{
				auto boxed = sel.Tag();
				if (boxed) tag = unbox_value_or<hstring>(boxed, L"");
			}
		}

		if (tag.empty()) return;

		// Convert tag to ElementTheme
		ElementTheme desired = ElementTheme::Default;
		if (tag == L"Light") desired = ElementTheme::Light;
		else if (tag == L"Dark") desired = ElementTheme::Dark;
		else desired = ElementTheme::Default;

		// Update theme using ThemeHelper (persists to storage)
		::OpenNet::Helpers::ThemeHelper::RootTheme(desired);

		// Apply to current window
		auto window = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowForElement(*this);
		if (window)
		{
			::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);
		}
	}

	void SettingsPage::SetDesktopBackground()
	{
		try
		{
			// 尝试读取系统壁纸路径；如果存在则设置为 BitmapImage，否则尝试读取纯色背景并设置 Border 背景色。
			auto img = DesktopBackgroundImage();
			auto border = DesktopBackgroundBorder();

			// 获取系统壁纸路径
			WCHAR wallpaperPathBuf[MAX_PATH]{};
			if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, wallpaperPathBuf, 0))
			{
				std::wstring wallpaperPath{ wallpaperPathBuf };

				// 判断文件是否存在
				bool fileExists = false;
				if (!wallpaperPath.empty())
				{
					auto attrs = GetFileAttributesW(wallpaperPath.c_str());
					fileExists = (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
				}

				if (fileExists && img)
				{
					try
					{
						// Construct file:/// URI, replacing backslashes with forward slashes.
						// URL-encode spaces and special characters using Win32 UrlCreateFromPathW.
						constexpr DWORD kMaxUrlLen = 2083;
						WCHAR encodedUrl[kMaxUrlLen]{};
						DWORD urlLen = kMaxUrlLen;
						HRESULT hr = UrlCreateFromPathW(wallpaperPath.c_str(), encodedUrl, &urlLen, 0);
						std::wstring uri;
						if (SUCCEEDED(hr))
						{
							uri = encodedUrl;
						}
						else
						{
							// Fallback: manual construction (may fail on special chars)
							uri = L"file:///" + wallpaperPath;
							std::replace(uri.begin(), uri.end(), L'\\', L'/');
						}

						winrt::Windows::Foundation::Uri fileUri{ uri };
						BitmapImage bitmap;
						bitmap.ImageFailed([](winrt::Windows::Foundation::IInspectable const&,
											  winrt::Microsoft::UI::Xaml::ExceptionRoutedEventArgs const& args)
						{
							OutputDebugStringW((L"SetDesktopBackground ImageFailed: " + args.ErrorMessage() + L"\n").c_str());
						});
						bitmap.UriSource(fileUri);
						img.Source(bitmap);

						OutputDebugStringW(L"壁纸已更改！\n");
					}
					catch (...)
					{
						OutputDebugStringW(L"SetDesktopBackground: BitmapImage creation failed\n");
					}
					return;
				}
			}

			// 如果没有可用壁纸，则尝试读取纯色背景（注册表：Control Panel\\Colors\\Background，格式 "R G B"）
			WCHAR colorBuf[128]{};
			DWORD bufSize = sizeof(colorBuf);
			LSTATUS status = RegGetValueW(HKEY_CURRENT_USER,
										  L"Control Panel\\Colors",
										  L"Background",
										  RRF_RT_REG_SZ,
										  nullptr,
										  colorBuf,
										  &bufSize);
			if (status == ERROR_SUCCESS && border)
			{
				std::wstring s{ colorBuf };
				// 解析 "R G B"
				int r = 0, g = 0, b = 0;
				std::wistringstream iss(s);
				if ((iss >> r >> g >> b))
				{
					::winrt::Windows::UI::Color color{};
					color.A = 255;
					color.R = static_cast<uint8_t>(std::clamp(r, 0, 255));
					color.G = static_cast<uint8_t>(std::clamp(g, 0, 255));
					color.B = static_cast<uint8_t>(std::clamp(b, 0, 255));

					auto brush = SolidColorBrush(color);
					border.Background(brush);

					OutputDebugStringW(L"纯色背景已设置！\n");
					return;
				}
			}
		}
		catch (...)
		{
			OutputDebugStringW(L"SetDesktopBackground: Unexpected exception\n");
		}
	}


	//void SettingsPage::AnnotatedScrollBarPage_Loaded(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	//{
	//	try
	//	{
	//		if (auto scrollView = SettingsScrollView())
	//		{
	//			if (auto presenter = scrollView.ScrollPresenter())
	//			{
	//				auto controller = annotatedScrollBar().ScrollController();
	//				if (controller)
	//				{
	//					presenter.VerticalScrollController(controller);
	//				}
	//			}
	//		}
	//	}
	//	catch (...) {}
	//}

	//void SettingsPage::AnnotatedScrollBarPage_Unloaded(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	//{
	//	// 解除控制器绑定，防止卸载后引用悬空
	//	try
	//	{
	//		if (auto scrollView = SettingsScrollView())
	//		{
	//			if (auto presenter = scrollView.ScrollPresenter())
	//			{
	//				presenter.VerticalScrollController(nullptr);
	//			}
	//		}
	//	}
	//	catch (...) {}
	//}

	//void SettingsPage::AnnotatedScrollBar_DetailLabelRequested(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::AnnotatedScrollBarDetailLabelRequestedEventArgs const& e)
	//{
	//	// Provide a string as the tooltip content when hovering the mouse over the AnnotatedScrollBar's vertical rail. The string simply
	//	// represents the color of the last item in the row computed from AnnotatedScrollBarDetailLabelRequestedEventArgs.ScrollOffset.
	//	// e.Content() = GetOffsetLabel(e.ScrollOffset());
	//	// 简单示例：展示偏移值；如需匹配示例，可按数据源计算标签文本
	//	// e.Content(box_value(hstring(L"Label")));
	//	e.Content(box_value(hstring(L"Offset: ") + to_hstring(e.ScrollOffset())));
	//}
	winrt::Windows::Foundation::IAsyncAction SettingsPage::OnSettingsPageLoadedAsync(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		auto weak = get_weak();

		co_await winrt::resume_background();

		try
		{
			m_initialLanguageOverride = ApplicationLanguages::PrimaryLanguageOverride();

			if (m_initialLanguageOverride.empty())
			{
				OutputDebugStringW(L"PrimaryLanguageOverride is empty(NOT SET)\n");
			}
		}
		catch (const winrt::hresult_error& e)
		{
#ifdef _DEBUG
			std::wstring msg = L"WinRT Exception: ";
			msg += e.message().c_str();
			msg += L"\n";
			OutputDebugStringW(msg.c_str());
#endif
		}
#ifdef _DEBUG
		OutputDebugStringW((m_initialLanguageOverride + L"\n").c_str());

#endif // _DEBUG

		co_await wil::resume_foreground(m_dispatcher);

		auto strong = weak.get();
		if (!strong) co_return;

		try
		{
			// Initialize SoftLanguageCombobox with supported languages
			if (auto comboLang = SoftLanguageCombobox())
			{
				comboLang.Items().Clear();

				auto supported = ApplicationLanguages::Languages();
				int32_t index = 0;
				int32_t selectedIndex = 0;

				auto autoItem = ComboBoxItem();
				autoItem.Content(box_value(hstring(L"Auto")));
				autoItem.Tag(box_value(hstring(L"")));
				comboLang.Items().Append(autoItem);
				index = 1;

				for (auto const& langTag : supported)
				{
					winrt::Windows::Globalization::Language lang{ langTag };
					auto display = lang.DisplayName();
					auto item = ComboBoxItem();
					item.Content(box_value(display));
					item.Tag(box_value(langTag));
					comboLang.Items().Append(item);

					if (ApplicationLanguages::PrimaryLanguageOverride() == langTag)
					{
						selectedIndex = index;
					}
					++index;
				}

				auto current = ApplicationLanguages::PrimaryLanguageOverride();
				comboLang.SelectedIndex(current.empty() ? 0 : selectedIndex);
			}

			// Initialize StartPageCombobox
			if (auto combo = StartPageCombobox())
			{
				combo.Items().Clear();

				struct PageEntry { const wchar_t* display; const wchar_t* tag; };
				PageEntry entries[] = {
					{ L"Home",     L"home" },
					{ L"Tasks",    L"tasks" },
					{ L"RSS",      L"rss" },
					{ L"Settings", L"settings" },
				};

				for (auto& e : entries)
				{
					auto item = ComboBoxItem();
					item.Content(box_value(hstring(e.display)));
					item.Tag(box_value(hstring(e.tag)));
					combo.Items().Append(item);
				}
			}

			// Initialize background and info bar
			SetDesktopBackground();
			m_hasPendingLangChange = false;
			if (auto infoBar = RestartLanguageInfoBar())
			{
				infoBar.IsOpen(false);
			}

			// Load current theme from ThemeHelper
			ElementTheme currentTheme = ::OpenNet::Helpers::ThemeHelper::RootTheme();

			// Set ComboBox selection based on saved theme
			if (auto combo = themeMode())
			{
				switch (currentTheme)
				{
					case ElementTheme::Light:  combo.SelectedIndex(0); break;
					case ElementTheme::Dark:   combo.SelectedIndex(1); break;
					default:                   combo.SelectedIndex(2); break;
				}
			}

			// Load saved start page and set ComboBox selection
			{
				m_isStartPageLoading = true;
				winrt::hstring savedTag = L"home";
				try
				{
					auto localSettings = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings();
					auto values = localSettings.Values();
					if (values.HasKey(L"StartPage"))
					{
						savedTag = unbox_value_or<hstring>(values.Lookup(L"StartPage"), L"home");
					}
				}
				catch (...) {}

				if (auto combo = StartPageCombobox())
				{
					for (int32_t i = 0; i < static_cast<int32_t>(combo.Items().Size()); ++i)
					{
						if (auto item = combo.Items().GetAt(i).try_as<ComboBoxItem>())
						{
							if (unbox_value_or<hstring>(item.Tag(), L"") == savedTag)
							{
								combo.SelectedIndex(i);
								break;
							}
						}
					}
				}
				m_isStartPageLoading = false;
			}
		}
		catch (...) {}

		m_loadAction = nullptr;
		co_return;
	}
	void SettingsPage::AppUpdateCheckButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{

	}
	void SettingsPage::AutoCheckUpdateCheckbox_Checked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
	}
	void SettingsPage::AutoCheckUpdateCheckbox_Unchecked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
	}
	void SettingsPage::goGithubButton_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		// Store the IAsyncAction to prevent unobserved async exception
		m_githubAction = GithubDefaultLaunch();
	}
}