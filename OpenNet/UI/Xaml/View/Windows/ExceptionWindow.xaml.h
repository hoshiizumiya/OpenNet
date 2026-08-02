#pragma once

import winrt.WinUI3Package;

#include "UI/Xaml/View/Windows/ExceptionWindow.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct ExceptionWindow : ExceptionWindowT<ExceptionWindow>
	{
		ExceptionWindow(hstring const& sentryId, hstring const& exception);

		void InitializeComponent();

		hstring TraceId();
		hstring Exception();
		hstring Comment();
		void Comment(hstring const& value);

		void ViewWindowExceptionCloseButton_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		static void Show(
			hstring const& sentryId,
			hstring const& exception);

	private:
		void InitializeWindow();
		winrt::fire_and_forget CloseWindowAsync();

		hstring m_sentryId;
		hstring m_exception;
		hstring m_comment;
		std::atomic_bool m_closeStarted{ false };
		bool m_allowClose{ false };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct ExceptionWindow : ExceptionWindowT<ExceptionWindow, implementation::ExceptionWindow>
	{
	};
}
