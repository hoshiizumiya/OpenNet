#include <sentry.h>

#include "XamlWorkaround.h"
#include "ExceptionWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/ExceptionWindow.g.cpp")
#include "UI/Xaml/View/Windows/ExceptionWindow.g.cpp"
#endif

import OpenNet.App;
import OpenNet.Core.ExceptionService.ExceptionFormat;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Windowing;
import winrtplus.Microsoft.UI.Interop;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	ExceptionWindow::ExceptionWindow(
		hstring const& sentryId,
		hstring const& exception)
		: m_sentryId(sentryId),
		m_exception(exception),
		m_comment(L"")
	{
		ExtendsContentIntoTitleBar(true);
	}

	void ExceptionWindow::InitializeComponent()
	{
		ExceptionWindowT::InitializeComponent();
		InitializeWindowExBase();
		InitializeWindow();
	}

	void ExceptionWindow::InitializeWindow()
	{
		AppWindow().Closing([weak = get_weak()](
			auto&&,
			winrt::Microsoft::UI::Windowing::
			AppWindowClosingEventArgs const& args)
		{
			if (auto self = weak.get())
			{
				if (!self->m_allowClose)
				{
					args.Cancel(true);
					self->CloseWindowAsync();
				}
			}
		});

		Closed([this](auto&&, auto&&)
		{
			this->get_strong();
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(Window());
			// The fatal event and optional feedback have been flushed.
			winrt::OpenNet::implementation::App::RequestExit();
		});

		SetTitleBar(ExceptionWindowTitleBar());

		winrt::Microsoft::UI::Xaml::Window ownerWindow =
			winrt::OpenNet::implementation::App::window
				? winrt::OpenNet::implementation::App::window.Window()
				: nullptr;
		if (!ownerWindow && winrt::OpenNet::implementation::App::guideWindow)
			ownerWindow = winrt::OpenNet::implementation::App::guideWindow.Window();
		bool hasOwner = false;
		if (ownerWindow)
		{
			HWND ownerHwnd =
				::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::
				GetWindowHandleFromWindow(ownerWindow);
			HWND ownedHwnd = reinterpret_cast<HWND>(Hwnd());

			if (ownerHwnd && ownedHwnd)
			{
				::SetWindowLongPtrW(
					ownedHwnd,
					GWLP_HWNDPARENT,
					reinterpret_cast<LONG_PTR>(ownerHwnd));
				hasOwner = true;
			}
		}

		if (hasOwner)
		{
			if (auto presenter = AppWindow().Presenter().try_as<
				winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
			{
				presenter.IsModal(true);
			}
		}
	}

	hstring ExceptionWindow::TraceId()
	{
		return hstring{ std::format(L"trace.id: {}", m_sentryId) };
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

	void ExceptionWindow::ViewWindowExceptionCloseButton_Click(
		winrt::Windows::Foundation::IInspectable const& /*sender*/,
		RoutedEventArgs const& /*e*/)
	{
		CloseWindowAsync();
	}

	winrt::fire_and_forget ExceptionWindow::CloseWindowAsync()
	{
		if (m_closeStarted.exchange(true))
		{
			co_return;
		}

		auto strong = get_strong();
		auto dispatcher = DispatcherQueue();
		ExceptionWindowCloseButton().IsEnabled(false);

		std::string comment;
		std::string exceptionDetails;
		sentry_uuid_t sentryUuid = sentry_uuid_nil();
		bool hasComment = false;
		try
		{
			comment = winrt::to_string(m_comment);
			exceptionDetails = winrt::to_string(m_exception);
			hasComment = std::ranges::any_of(
				m_comment,
				[](wchar_t value)
			{
				return value != L' '
					&& value != L'\t'
					&& value != L'\r'
					&& value != L'\n';
			});
			sentryUuid =
				::OpenNet::Core::ExceptionService::ExceptionFormat::
				ToSentryUuid(m_sentryId);
		}
		catch (...)
		{
		}

		// sentry_flush blocks, so all Sentry I/O runs off the UI thread.
		co_await winrt::resume_background();

		if (hasComment)
		{
			auto const associatedEventId =
				sentry_uuid_is_nil(&sentryUuid)
				? nullptr
				: &sentryUuid;
			sentry_value_t userFeedback = sentry_value_new_feedback(
				comment.c_str(),
				nullptr,
				nullptr,
				associatedEventId);
			sentry_hint_t* hint = sentry_hint_new();
			if (hint && !exceptionDetails.empty())
			{
				sentry_hint_attach_bytes(
					hint,
					exceptionDetails.data(),
					exceptionDetails.size(),
					"exception-details.txt");
			}
			if (sentry_scope_t* feedbackScope = sentry_local_scope_new())
			{
				sentry_scope_set_tag(
					feedbackScope,
					"feedback.source",
					"ExceptionWindow");
				sentry_scope_capture_feedback(
					feedbackScope,
					userFeedback,
					hint);
			}
			else
			{
				sentry_capture_feedback_with_hint(userFeedback, hint);
			}
		}

		// Flush even without feedback so the fatal event is sent before exit.
		sentry_flush(5000);

		if (!dispatcher || !dispatcher.TryEnqueue([strong]()
		{
			strong->m_allowClose = true;
			strong->Close();
		}))
		{
			ExitProcess(EXIT_FAILURE);
		}
	}

	void ExceptionWindow::Show(
		hstring const& sentryId,
		hstring const& exception)
	{
		auto window = winrt::make<ExceptionWindow>(sentryId, exception);
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		window.Activate();
	}
}
