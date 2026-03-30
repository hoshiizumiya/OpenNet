#include "pch.h"
#include "ExceptionWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/ExceptionWindow.g.cpp")
#include "UI/Xaml/View/Windows/ExceptionWindow.g.cpp"
#endif

#include "App.xaml.h"
#include "Helpers/ThemeHelper.h"
#include "Helpers/WindowHelper.h"
#include <format>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	ExceptionWindow::ExceptionWindow(GUID const& sentryId, hstring const& exception)
		: m_sentryId(sentryId), m_exception(exception), m_comment(L"")
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
		wchar_t guidStr[37];
		swprintf_s(guidStr, sizeof(guidStr) / sizeof(wchar_t),
			L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			m_sentryId.Data1,
			m_sentryId.Data2,
			m_sentryId.Data3,
			m_sentryId.Data4[0], m_sentryId.Data4[1],
			m_sentryId.Data4[2], m_sentryId.Data4[3],
			m_sentryId.Data4[4], m_sentryId.Data4[5],
			m_sentryId.Data4[6], m_sentryId.Data4[7]);
		return winrt::hstring(std::format(L"trace.id: {}", guidStr));
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

	void ExceptionWindow::ViewWindowExceptionCloseButton_Click(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
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
				sentry_value_t feedback = sentry_value_new_object();
				sentry_value_set_by_key(feedback, "comments", sentry_value_new_string(winrt::to_string(m_comment).c_str()));
				sentry_value_set_by_key(feedback, "level", sentry_value_new_string("info"));
				sentry_capture_feedback(feedback);
			}

			// Flush events to Sentry
			sentry_flush(5000); // 5 second timeout
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

	void ExceptionWindow::Show(GUID const& sentryId, hstring const& exception)
	{
		auto window = winrt::make<ExceptionWindow>(sentryId, exception);
		window.AppWindow().Show();
		window.AppWindow().MoveInZOrderAtTop();
	}
}
