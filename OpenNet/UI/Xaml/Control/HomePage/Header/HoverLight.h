#pragma once

#include "UI/Xaml/Control/HomePage/Header/HoverLight.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	struct HoverLight : HoverLightT<HoverLight>
	{
		HoverLight() = default;

		void OnConnected(winrt::Microsoft::UI::Xaml::UIElement const& newElement);
		void OnDisconnected(winrt::Microsoft::UI::Xaml::UIElement const& oldElement);
		winrt::hstring GetId();

	private:
		void TargetElement_PointerMoved(winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void TargetElement_PointerExited(winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void MoveToRestingPosition();

		winrt::Microsoft::UI::Composition::ExpressionAnimation m_lightPositionExpression{ nullptr };
		winrt::Microsoft::UI::Composition::Vector3KeyFrameAnimation m_offsetAnimation{ nullptr };
		winrt::event_token m_pointerMovedToken{};
		winrt::event_token m_pointerExitedToken{};
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::factory_implementation
{
	struct HoverLight : HoverLightT<HoverLight, implementation::HoverLight>
	{
	};
}
