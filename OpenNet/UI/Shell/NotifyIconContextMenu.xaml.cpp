#include "XamlWorkaround.h"
#include "NotifyIconContextMenu.xaml.h"
#if __has_include("UI/Shell/NotifyIconContextMenu.g.cpp")
#include "UI/Shell/NotifyIconContextMenu.g.cpp"
#endif

#include "NotifyIconXamlHostWindow.xaml.h"

import OpenNet.App;
import OpenNet.Core.AppRuntime;
import OpenNet.Helpers.WindowHelper;
import winrt.WinUI3Package;
import winrt.Windows.ApplicationModel;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Windows.Foundation;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;

namespace winrt::OpenNet::UI::Shell::implementation
{
	winrt::hstring NotifyIconContextMenu::Title() const
	{
		return ::OpenNet::Core::AppRuntime::GetDisplayName();
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::CloseNotifyIconContextMenuWindowButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		try
		{
			Grid root = Root();

			root.RenderTransformOrigin({ 0.5f, 0.5f });

			ScaleTransform scaleTransform;

			scaleTransform.ScaleX(1.0);
			scaleTransform.ScaleY(1.0);

			root.RenderTransform(scaleTransform);

			Storyboard storyboard;

			auto createAnimation =
				[&](double to, std::wstring_view property)
			{
				DoubleAnimation animation;

				animation.To(to);

				animation.Duration(
					DurationHelper::FromTimeSpan(
						std::chrono::milliseconds(120)));

				animation.EnableDependentAnimation(true);

				Storyboard::SetTarget(animation, root);

				Storyboard::SetTargetProperty(
					animation,
					property);

				return animation;
			};

			storyboard.Children().Append(
				createAnimation(
					0.0,
					L"Opacity"));

			storyboard.Children().Append(
				createAnimation(
					0.95,
					L"(UIElement.RenderTransform).(ScaleTransform.ScaleX)"));

			storyboard.Children().Append(
				createAnimation(
					0.95,
					L"(UIElement.RenderTransform).(ScaleTransform.ScaleY)"));

			storyboard.Begin();

			co_await winrt::resume_after(
				std::chrono::milliseconds(120));
		}
		catch (...)
		{
		}

		m_notifyIconContextMenu.Hide();
	}

	void NotifyIconContextMenu::ExitApplication()
	{
		// Remove the tray icon
		auto trayIcon = winrt::OpenNet::implementation::App::trayIcon;
		trayIcon->Remove();

		// Allow the window to close (bypasses the hide-to-tray Closing handler)
		winrt::OpenNet::implementation::App::s_isExiting = true;
		// For test now
		auto window = winrt::OpenNet::implementation::App::window;
		if (window)
		{
			window.Close();
		}

		// Exit the application - now the Closing handler will not cancel the close,
		// the window closes properly, App::~App() runs, and all services shut down.
	}

	void NotifyIconContextMenu::HomeAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::ShowMainWindow();
	}

	void NotifyIconContextMenu::ExitAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		//auto app = winrt::OpenNet::App();
		//app.Exit();
		Microsoft::UI::Xaml::Application::Current().Exit();
		//ExitProcess(0);

	}
}
