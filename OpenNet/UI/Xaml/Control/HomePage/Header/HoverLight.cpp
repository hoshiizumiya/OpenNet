#include "pch.h"
#include "HoverLight.h"
#if __has_include("UI/Xaml/Control/HomePage/Header/HoverLight.g.cpp")
#include "UI/Xaml/Control/HomePage/Header/HoverLight.g.cpp"
#endif

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Windows.UI.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Hosting;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	static const winrt::hstring s_hoverLightId{ L"OpenNet.UI.Xaml.Control.HomePage.Header.HoverLight" };

	void HoverLight::OnConnected(UIElement const& targetElement)
	{
		auto compositor = CompositionTarget::GetCompositorForCurrentThread();

		// Create SpotLight
		auto spotLight = compositor.CreateSpotLight();
		spotLight.InnerConeAngleInDegrees(50.0f);
		spotLight.InnerConeColor(winrt::Windows::UI::Colors::FloralWhite());
		spotLight.OuterConeAngleInDegrees(20.0f);
		spotLight.ConstantAttenuation(1.0f);
		spotLight.LinearAttenuation(0.253f);
		spotLight.QuadraticAttenuation(0.58f);

		this->CompositionLight(spotLight);

		// Resting position animation
		Windows::Foundation::Numerics::float3 restingPosition{ 200.0f, 200.0f, 400.0f };
		auto cbEasing = compositor.CreateCubicBezierEasingFunction({ 0.3f, 0.7f }, { 0.9f, 0.5f });
		m_offsetAnimation = compositor.CreateVector3KeyFrameAnimation();
		m_offsetAnimation.InsertKeyFrame(1.0f, restingPosition, cbEasing);
		m_offsetAnimation.Duration(std::chrono::milliseconds(500));

		spotLight.Offset(restingPosition);

		// Expression animation linking light offset to pointer position
		auto hoverPosition = ElementCompositionPreview::GetPointerPositionPropertySet(targetElement);
		m_lightPositionExpression = compositor.CreateExpressionAnimation(
			L"Vector3(hover.Position.X, hover.Position.Y, height)");
		m_lightPositionExpression.SetReferenceParameter(L"hover", hoverPosition);
		m_lightPositionExpression.SetScalarParameter(L"height", 100.0f);

		// Subscribe events
		m_pointerMovedToken = targetElement.PointerMoved({ this, &HoverLight::TargetElement_PointerMoved });
		m_pointerExitedToken = targetElement.PointerExited({ this, &HoverLight::TargetElement_PointerExited });

		XamlLight::AddTargetElement(GetId(), targetElement);
	}

	void HoverLight::MoveToRestingPosition()
	{
		if (this->CompositionLight())
		{
			this->CompositionLight().StartAnimation(L"Offset", m_offsetAnimation);
		}
	}

	void HoverLight::TargetElement_PointerMoved(IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
	{
		if (!this->CompositionLight()) return;

		if (e.Pointer().PointerDeviceType() == winrt::Microsoft::UI::Input::PointerDeviceType::Touch)
		{
			auto offset = e.GetCurrentPoint(sender.as<UIElement>()).Position();
			if (auto light = this->CompositionLight().try_as<SpotLight>())
			{
				light.Offset({ static_cast<float>(offset.X), static_cast<float>(offset.Y), 15.0f });
			}
		}
		else
		{
			this->CompositionLight().StartAnimation(L"Offset", m_lightPositionExpression);
		}
	}

	void HoverLight::TargetElement_PointerExited(IInspectable const&,
		winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
	{
		MoveToRestingPosition();
	}

	void HoverLight::OnDisconnected(UIElement const& oldElement)
	{
		oldElement.PointerMoved(m_pointerMovedToken);
		oldElement.PointerExited(m_pointerExitedToken);

		XamlLight::RemoveTargetElement(GetId(), oldElement);

		if (this->CompositionLight())
		{
			this->CompositionLight().Close();
		}

		m_lightPositionExpression = nullptr;
		m_offsetAnimation = nullptr;
	}

	hstring HoverLight::GetId()
	{
		return s_hoverLightId;
	}
}
