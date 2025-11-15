#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("Pages/SettingsPages/SettingsPage.g.cpp")
#include "Pages/SettingsPages/SettingsPage.g.cpp"
#endif

#include "Folder.h"
#include "MainSettingsPage.xaml.h"
#include "../../MainWindow.xaml.h"
#include "../../Helpers/WindowHelper.h"
#include "winrt/Microsoft.Windows.AppLifecycle.h"
// 解析 BitmapImage 的构造函数实现
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

// 用于解析字符串为整数
#include <sstream>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Microsoft::UI::Xaml::Media::Animation;
using namespace Windows::Globalization;
using namespace Microsoft::Windows::ApplicationModel::Resources;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	// 正确定义类静态成员（不要使用文件作用域的 static 关键字）
	SettingsPage* SettingsPage::s_current = nullptr;

	SettingsPage::SettingsPage()
	{
		InitializeComponent();
		s_current = this;

		// Capture initial language override at page creation
		m_initialLanguageOverride = ApplicationLanguages::PrimaryLanguageOverride();
#ifdef _DEBUG
		OutputDebugString(ApplicationLanguages::PrimaryLanguageOverride().c_str());

#endif // _DEBUG

		// m_initialLanguageOverride = L"";
		// 不要把初始值强制设为空，否则将导致重启后也被判定为“与初始不同”

		// Build ComboBox items from ApplicationLanguages::Languages()
		try
		{
			// Clear existing items if any
			SoftLanguageCombobox().Items().Clear();

			auto supported = ApplicationLanguages::Languages(); // IVectorView<hstring>
			int32_t index = 0;
			int32_t selectedIndex = 0; // default to Auto fallback

			// Add an explicit "Auto" entry at index 0
			auto autoItem = ComboBoxItem();
			auto autoText = hstring(L"Auto");
			autoItem.Content(box_value(autoText));
			autoItem.Tag(box_value(hstring(L""))); // empty tag means follow system
			SoftLanguageCombobox().Items().Append(autoItem);
			index = 1;

			// Append each supported language with its display name and tag
			for (auto const& langTag : supported)
			{
				winrt::Windows::Globalization::Language lang{ langTag };
				auto display = lang.DisplayName();
				// Create ComboBoxItem with Content = display name, Tag = language tag
				auto item = ComboBoxItem();
				item.Content(box_value(display));
				item.Tag(box_value(langTag));
				SoftLanguageCombobox().Items().Append(item);

				// If this tag matches PrimaryLanguageOverride, remember index
				if (ApplicationLanguages::PrimaryLanguageOverride() == langTag)
				{
					selectedIndex = index;
				}

				++index;
			}

			// Set selected index (if PrimaryLanguageOverride empty, keep 0 = Auto)
			auto current = ApplicationLanguages::PrimaryLanguageOverride();
			if (current.empty())
			{
				SoftLanguageCombobox().SelectedIndex(0);
			}
			else
			{
				SoftLanguageCombobox().SelectedIndex(selectedIndex);
			}
		}
		catch (...)
		{
			SoftLanguageCombobox().SelectedIndex(0);
		}


		Loaded({ this, &SettingsPage::AnnotatedScrollBarPage_Loaded });
		Unloaded({ this, &SettingsPage::AnnotatedScrollBarPage_Unloaded });
		SetDesktopBackground();

		// 启动时根据当前状态关闭“需要重启”提示
		m_hasPendingLangChange = false;
		if (auto infoBar = RestartLanguageInfoBar())
		{
			infoBar.IsOpen(false);
		}
	}

	void SettingsPage::Aboutp_Click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		// --- ISSUE AND FIX ---
		// The original code created one 'Folder' object and added it twice,
		// changing its name in between. This results in the BreadcrumbBar
		// showing two items that both point to the same object, so both display the last name set ("About").
		//
		// The fix is to create two distinct 'Folder' objects.

		// 1. Get the current items from the MainSettingsPage's BreadcrumbBar.
		auto mainSettingsPage = MainSettingsPage::Current();
		if (!mainSettingsPage) return;

		auto itemsSource = mainSettingsPage->MainSettingsPageBar().ItemsSource();
		auto breadcrumbItems = itemsSource.try_as<IObservableVector<IInspectable>>();
		if (!breadcrumbItems)
		{
			// If the source is not an IObservableVector or is null, create a new one.
			// This makes the code more robust.
			breadcrumbItems = single_threaded_observable_vector<IInspectable>();
			mainSettingsPage->MainSettingsPageBar().ItemsSource(breadcrumbItems);
		}


		// 2. Clear existing items beyond the root "Settings" item if necessary.
		// This handles cases where the user might navigate elsewhere before coming here.
		// A simpler approach is to just rebuild the list. Let's do that.
		breadcrumbItems.Clear();

		// 3. Create and add the "Settings" folder.
		auto settingsFolder = winrt::make<winrt::OpenNet::Pages::SettingsPages::implementation::Folder>();
		settingsFolder.Name(L"Settings"); // Assuming you have a Folder class with a Name property.
		breadcrumbItems.Append(settingsFolder);


		// 4. Create and add the "About" folder. This is the new, separate object.
		auto aboutFolder = winrt::make<winrt::OpenNet::Pages::SettingsPages::implementation::Folder>();
		aboutFolder.Name(L"About");
		breadcrumbItems.Append(aboutFolder);


		// 不能写成 SlideNavigationTransitionInfo{ Effect = SlideNavigationTransitionEffect::FromRight };
		// 这是 C# 的属性初始化语法，C++ 不支持，因此会导致 “未定义标识符 Effect”。
		// 正确写法：
		auto transitionInfo = SlideNavigationTransitionInfo{};
		transitionInfo.Effect(SlideNavigationTransitionEffect::FromRight);

		MainSettingsPage::Current()->SettingsFrame().Navigate(
			xaml_typename<winrt::OpenNet::Pages::SettingsPages::AboutPage>(),
			nullptr,
			transitionInfo
		);
	}

	void SettingsPage::SPSettings_Click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		// Placeholder for serial port settings click
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
		// Start page selection handling (not implemented)
	}

	void SettingsPage::SetDesktopBackground()
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
				// 构造 file:/// URI 并替换反斜杠为正斜杠
				std::wstring uri = L"file:///" + wallpaperPath;
				std::replace(uri.begin(), uri.end(), L'\\', L'/');

				// 创建 BitmapImage 并设置为 Image.Source
				winrt::Windows::Foundation::Uri fileUri{ uri };
				BitmapImage bitmap{ fileUri };
				img.Source(bitmap);

				OutputDebugStringW(L"壁纸已更改！\n");
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
				Windows::UI::Color color{};
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


	void SettingsPage::AnnotatedScrollBarPage_Loaded(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		SettingsScrollView().ScrollPresenter().VerticalScrollController() = annotatedScrollBar().ScrollController();
	}

	void SettingsPage::AnnotatedScrollBarPage_Unloaded(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		// 解除控制器绑定，防止卸载后引用悬空
		//if (auto presenter = SettingsScrollView().ScrollPresenter())
		//{
		//	presenter.VerticalScrollController(nullptr);
		//}

	}

	void SettingsPage::AnnotatedScrollBar_DetailLabelRequested(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::AnnotatedScrollBarDetailLabelRequestedEventArgs const& e)
	{
		// Provide a string as the tooltip content when hovering the mouse over the AnnotatedScrollBar's vertical rail. The string simply
		// represents the color of the last item in the row computed from AnnotatedScrollBarDetailLabelRequestedEventArgs.ScrollOffset.
		// e.Content() = GetOffsetLabel(e.ScrollOffset());
		// 简单示例：展示偏移值；如需匹配示例，可按数据源计算标签文本
		// e.Content(box_value(hstring(L"Label")));
		e.Content(box_value(hstring(L"Offset: ") + to_hstring(e.ScrollOffset())));
	}
}