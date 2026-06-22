#include <sentry.h>

#include "XamlWorkaround.h"
#include "ExceptionWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/ExceptionWindow.g.cpp")
#include "UI/Xaml/View/Windows/ExceptionWindow.g.cpp"
#endif

import OpenNet.App;
import OpenNet.Core.ExceptionService.ExceptionFormat;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Windows.Graphics;
import winrt.Microsoft.UI;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Windowing;
import winrtplus.Microsoft.UI.Interop;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	ExceptionWindow::ExceptionWindow(winrt::guid const& sentryId, hstring const& exception) :
		m_sentryId(::OpenNet::Core::ExceptionService::ExceptionFormat::ToSentryUuid(sentryId)),
		m_exception(exception),
		m_comment(L"")
	{
		InitializeComponent();
		InitializeWindow();
	}

	void ExceptionWindow::InitializeWindow()
	{
		AppWindow().Title(L"OpenNet Exception Report");

		auto titleBar = AppWindow().TitleBar();
		titleBar.IconShowOptions(winrt::Microsoft::UI::Windowing::IconShowOptions::HideIconAndSystemMenu);
		ExtendsContentIntoTitleBar(true);

		Closed([](auto&&, auto&&)
		{
			// Close the application on exception window close
			winrt::Microsoft::UI::Xaml::Application::Current().Exit();
		});

		AppWindow().Resize(winrt::Windows::Graphics::SizeInt32(800, 400));
		AppWindow().MoveInZOrderAtTop();
		::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(*this);

		SetTitleBar(ExceptionWindowTitleBar());
		AppWindow().TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);

		auto const& ownerWindow = winrt::OpenNet::implementation::App::window;
		if (ownerWindow)
		{
			HWND ownerHwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(ownerWindow);
			auto ownedWindowId = AppWindow().Id();
			HWND ownedHwnd = winrt::Microsoft::UI::GetWindowFromWindowId(ownedWindowId);

			if (ownerHwnd && ownedHwnd)
			{
				::SetWindowLongPtrW(ownedHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(ownerHwnd));
			}
		}

		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);

		if (auto presenter = winrt::Microsoft::UI::Windowing::OverlappedPresenter::CreateForDialog())
		{
			presenter.IsModal(true);
			presenter.IsResizable(true);
			presenter.IsMaximizable(true);
			AppWindow().SetPresenter(presenter);
		}
		AppWindow().Show();
	}

	hstring ExceptionWindow::TraceId()
	{
		// Convert GUID to string format
		char guidStr[37];
		sentry_uuid_as_string(&m_sentryId, guidStr);
		return winrt::hstring(std::format(L"trace.id: {}", winrt::to_hstring(guidStr)));
	}

	hstring ExceptionWindow::Exception()
	{
		return m_exception;
	}

	hstring ExceptionWindow::Comment()
	{
		return m_comment;
	}

	void ExceptionWindow::Comment(hstring const& value)
	{
		m_comment = value;
	}

	void ExceptionWindow::ViewWindowExceptionCloseButton_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
	{
		CloseWindowAsync();
	}

	winrt::fire_and_forget ExceptionWindow::CloseWindowAsync()
	{
		// Switch to background thread for Sentry operations
		co_await winrt::resume_background();

		try
		{
			// Submit feedback if comment is provided
			if (!m_comment.empty())
			{
				sentry_value_t user_feedback = sentry_value_new_feedback(
					winrt::to_string(m_comment).c_str(), nullptr, nullptr, &m_sentryId);
				sentry_capture_feedback(user_feedback);

				// Flush events to Sentry
				sentry_flush(5000); // 5 second timeout
			}

		}
		catch (...)
		{
			// Silently ignore errors during Sentry operations
		}

		// Switch back to UI thread to close the window
		auto dispatcher = DispatcherQueue();
		if (dispatcher)
		{
			dispatcher.TryEnqueue([this]()
			{
				Close();
			});
		}
	}

	void ExceptionWindow::Show(winrt::guid const& sentryId, hstring const& exception)
	{
		auto window = winrt::make<ExceptionWindow>(sentryId, exception);
		window.AppWindow().Show();
		window.AppWindow().MoveInZOrderAtTop();
	}
}
