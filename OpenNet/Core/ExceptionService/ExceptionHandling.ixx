module;
#include <sentry.h>
#include <wil/result.h>

export module OpenNet.Core.ExceptionService.ExceptionHandling;

import OpenNet.Core.ExceptionService.OpenNetException;
import OpenNet.Core.ExceptionService.CapturedException;
import OpenNet.Core.Utils.Message;
import OpenNet.XamlApplicationLifetime;
import winrt_base;
import winrt.Windows.Foundation;
import winrt.Microsoft.UI.Xaml;
import std;

export namespace OpenNet::Core::ExceptionService
{
	class ExceptionHandling
	{
	public:
		/// <summary>
		/// Kill the current process if the exception is or has a DbException.
		/// As this method does not throw, it should only be used in catch blocks
		/// </summary>
		/// <param name="exception">Incoming exception</param>
		/// <returns>Unwrapped DbException or original exception</returns>
		[[noreturn]]
		static void KillProcessOnDatabaseException(OpenNetException const& ex)
		{
			OpenNet::Core::Utils::Message::ShowErrorMessage(
				L"Database Error",
				ex.Message().data());

			ExitProcess(EXIT_FAILURE);
		}

		static void OnAppUnhandledException(winrt::Windows::Foundation::IInspectable sender, winrt::Microsoft::UI::Xaml::UnhandledExceptionEventArgs e)
		{
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
			if (IsDebuggerPresent())
			{
				auto errorMessage = e.Message();
				OutputDebugStringW((L"UnhandledException: " + std::wstring(errorMessage.c_str()) + L"\r\n").c_str());
			__debugbreak();
			}
#endif
			// Gets the HRESULT code associated with the unhandled exception.
			winrt::hresult exception = e.Exception();
			OpenNet::XamlApplicationLifetime::Exiting = true;

			KillProcessOnDatabaseException(OpenNetException(e.Exception()));

			// https://docs.sentry.io/platforms/native/usage/#capturing-errors
			sentry_value_t event = sentry_value_new_event();

			sentry_value_t exc = sentry_value_new_exception("Microsoft.UI.Xaml.UnhandledException", winrt::to_string(e.Message().c_str()).c_str());
			sentry_value_set_stacktrace(exc, NULL, 0);
			// add the exception to the event
			sentry_event_add_exception(event, exc);

			sentry_uuid_t id = sentry_capture_event(event);
			// SentrySdk.Flush();

			// Handled has to be set to true, the control flow is returned after post
			e.Handled(true);

			if (OpenNet::XamlApplicationLifetime::Exited)
			{
				return;
			}

			// TODO: Maybe we should close current xaml window because the message pump is still alive.
			// And user can still interact with the UI without any problems.
			CapturedException capturedException = CapturedException(id, exception);

			// Post(static state = > ExceptionWindow.Show(Unsafe.Unbox<CapturedException>(state!)), capturedException);

		}
	};
}