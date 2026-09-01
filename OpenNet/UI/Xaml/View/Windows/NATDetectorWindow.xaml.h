#pragma once

#include "Core/NetworkDetector.h"
#include "UI/Xaml/View/Windows/NATDetectorWindow.g.h"

import winrt.Windows.Foundation;
import OpenNet.Helpers.WindowExBase;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct NATDetectorWindow : NATDetectorWindowT<NATDetectorWindow>, WindowExBase<NATDetectorWindow>
	{
		NATDetectorWindow();

		winrt::Windows::Foundation::IAsyncAction StartButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction QuickPortButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void CloseButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		void InitializeWindow();
		void UpdateLibtorrentState();
		void ResetObservationTable();
		void ShowObservation(std::size_t index, ::OpenNet::Core::StunObservation const& observation);
		void ShowPortProbeResult(::OpenNet::Core::PortProbeResult const& result);

		::OpenNet::Core::NetworkDetector m_detector;
		bool m_running{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct NATDetectorWindow : NATDetectorWindowT<NATDetectorWindow, implementation::NATDetectorWindow>
	{
	};
}
