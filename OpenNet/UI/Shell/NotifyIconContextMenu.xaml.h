#pragma once

#include "UI/Shell/NotifyIconContextMenu.g.h"
#include <winrt/WinUI3Package.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>

namespace winrt::OpenNet::UI::Shell::implementation
{
	struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu>
	{
	private:
		// Context menu flyout
		Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase m_notifyIconContextMenu{ nullptr };
		// Root grid of the XAML page
		winrt::Microsoft::UI::Xaml::FrameworkElement m_notifyIconContextMenuRoot{ nullptr };
	public:
		NotifyIconContextMenu()
		{
			InitializeComponent();
			TitleText().Text(Title());
			m_notifyIconContextMenu = *this;
			m_notifyIconContextMenu.Closed([this](auto&&, auto&&)
			{
				Root().Opacity(1.0);

				if (auto st =
					Root().RenderTransform().try_as<winrt::Microsoft::UI::Xaml::Media::ScaleTransform>())
				{
					st.ScaleX(1.0);
					st.ScaleY(1.0);
				}
			});
		}


		winrt::hstring Title() const;
		winrt::Windows::Foundation::IAsyncAction CloseNotifyIconContextMenuWindowButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

		// Tray icon control
		void ExitApplication();
		void HomeAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void ExitAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
	};
}

namespace winrt::OpenNet::UI::Shell::factory_implementation
{
	struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu, implementation::NotifyIconContextMenu>
	{
	};
}
