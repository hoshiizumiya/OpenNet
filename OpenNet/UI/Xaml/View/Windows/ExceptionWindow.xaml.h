#pragma once

#include "UI/Xaml/View/Windows/ExceptionWindow.g.h"
#include <sentry.h>

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct ExceptionWindow : ExceptionWindowT<ExceptionWindow>
	{
		ExceptionWindow(GUID const& sentryId, hstring const& exception);

		void InitializeWindow();

		hstring TraceId();
		hstring Exception();
		hstring Comment();
		void Comment(hstring const& value);

		void ViewWindowExceptionCloseButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		static void Show(GUID const& sentryId, hstring const& exception);

	private:
		winrt::fire_and_forget CloseWindowAsync();

		GUID m_sentryId;
		hstring m_exception;
		hstring m_comment;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct ExceptionWindow : ExceptionWindowT<ExceptionWindow, implementation::ExceptionWindow>
	{
	};
}
