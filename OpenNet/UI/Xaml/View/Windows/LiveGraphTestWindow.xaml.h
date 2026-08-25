#pragma once

import OpenNet.Helpers.WindowExBase;
import std;
import winrt.Microsoft.Graphics.Canvas.UI.Xaml;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.OpenNet.UI.Xaml.Control.Graph;
import winrt.WinUI3Package;

#include "UI/Xaml/View/Windows/LiveGraphTestWindow.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct LiveGraphTestWindow : LiveGraphTestWindowT<LiveGraphTestWindow>, WindowExBase<LiveGraphTestWindow>
	{
		LiveGraphTestWindow();

		void DynamicGraph_CreateResources(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl const& canvas);
		void StaticGraph_CreateResources(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl const& canvas);
		void DynamicGraph_Draw(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::OpenNet::UI::Xaml::Control::Graph::LiveGraphEventArgs const& args);
		void DynamicGraph_HighlightLineUpdated(
			winrt::Windows::Foundation::IInspectable const& sender,
			float value);
		void StaticGraph_HighlightLineUpdated(
			winrt::Windows::Foundation::IInspectable const& sender,
			float value);

		void ResetDynamicGraph_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void AddStaticLine_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void AddStaticRounded_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void GreenBrush_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void BlueBrush_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void RedBrush_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void PurpleBrush_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void CustomBrush_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		void BackgroundMode_SelectionChanged(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
		void HighlightBehavior_SelectionChanged(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
		void GraphSettings_ValueChanged(
			winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender,
			winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
		void BrushAppearance_ValueChanged(
			winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender,
			winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
		void HighlightVisibility_Toggled(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void CustomColors_Toggled(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void BrushAppearance_Toggled(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		using GraphBrushData =
			winrt::OpenNet::UI::Xaml::Control::Graph::GraphBrushData;

		void RecreateDynamicStreams(
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl const& canvas);
		void ApplyGraphSettings();
		void ApplyBrushAppearance();
		void SetPrimaryBrush(GraphBrushData const& brush);
		void AddStaticBatch(bool rounded);
		void UpdateGradientBounds(GraphBrushData const& brush, float height);
		void DisposeBrush(GraphBrushData const& brush) noexcept;

		bool m_uiReady{};
		winrt::hstring m_primaryGraphKey;
		winrt::hstring m_secondaryGraphKey;
		winrt::hstring m_staticGraphKey;
		GraphBrushData m_primaryBrush{ nullptr };
		GraphBrushData m_secondaryBrush{ nullptr };
		GraphBrushData m_staticBrush{ nullptr };
		std::uint64_t m_drawFrame{};
		float m_phase{};
		double m_dynamicSampleElapsed{};
		double m_dynamicStatusElapsed{};
		std::atomic<double> m_dynamicScrollPixelsPerSecond{ 60.0 };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct LiveGraphTestWindow :
		LiveGraphTestWindowT<LiveGraphTestWindow, implementation::LiveGraphTestWindow>
	{
	};
}
