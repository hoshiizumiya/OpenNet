#pragma once

#include "UI/Xaml/Control/HomePage/Header/AmbLight.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	struct AmbLight : AmbLightT<AmbLight>
	{
		AmbLight() = default;

		void OnConnected(winrt::Microsoft::UI::Xaml::UIElement const& newElement);
		void OnDisconnected(winrt::Microsoft::UI::Xaml::UIElement const& oldElement);
		winrt::hstring GetId();
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::factory_implementation
{
	struct AmbLight : AmbLightT<AmbLight, implementation::AmbLight>
	{
	};
}
