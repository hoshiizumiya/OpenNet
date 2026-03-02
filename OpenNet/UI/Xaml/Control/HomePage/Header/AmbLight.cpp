#include "pch.h"
#include "AmbLight.h"
#if __has_include("UI/Xaml/Control/HomePage/Header/AmbLight.g.cpp")
#include "UI/Xaml/Control/HomePage/Header/AmbLight.g.cpp"
#endif

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.UI.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	static const winrt::hstring s_ambLightId{ L"OpenNet.UI.Xaml.Control.HomePage.Header.AmbLight" };

	void AmbLight::OnConnected(UIElement const& newElement)
	{
		auto compositor = CompositionTarget::GetCompositorForCurrentThread();

		auto ambientLight = compositor.CreateAmbientLight();
		ambientLight.Color(winrt::Windows::UI::Colors::White());

		this->CompositionLight(ambientLight);
		XamlLight::AddTargetElement(GetId(), newElement);
	}

	void AmbLight::OnDisconnected(UIElement const& oldElement)
	{
		XamlLight::RemoveTargetElement(GetId(), oldElement);
		if (this->CompositionLight())
		{
			this->CompositionLight().Close();
		}
	}

	hstring AmbLight::GetId()
	{
		return s_ambLightId;
	}
}
