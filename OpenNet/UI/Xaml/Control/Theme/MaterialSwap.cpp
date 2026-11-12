#include "XamlWorkaround.h"
#include "MaterialSwap.h"
#include "MaterialAppearance.h"
#include "MaterialControlState.h"
#include "ControlAppearance.h"
#include "MaterialControlBridge.h"
#include "LiquidGlassProfile.h"
#if __has_include("UI/Xaml/Control/Theme/MaterialSwap.g.cpp")
#include "UI/Xaml/Control/Theme/MaterialSwap.g.cpp"
#endif
import OpenNet.XamlApplicationLifetime;

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
	using namespace Microsoft::UI::Xaml;
	MaterialSwap::MaterialSwap()
	{
		HorizontalContentAlignment(HorizontalAlignment::Stretch);
		VerticalContentAlignment(VerticalAlignment::Stretch);
		IsTabStop(false);
		Loaded({ this, &MaterialSwap::LoadedHandler });
		Unloaded({ this, &MaterialSwap::UnloadedHandler });
	}
	DependencyProperty MaterialSwap::NativeProperty()
	{
		static auto property = DependencyProperty::Register(L"Native", xaml_typename<Windows::Foundation::IInspectable>(), xaml_typename<class_type>(), PropertyMetadata{ nullptr, PropertyChangedCallback{ Changed } });
		return property;
	}
	Windows::Foundation::IInspectable MaterialSwap::Native() const
	{
		return GetValue(NativeProperty());
	}
	void MaterialSwap::Native(Windows::Foundation::IInspectable const& value)
	{
		SetValue(NativeProperty(), value);
	}
	DependencyProperty MaterialSwap::GlassProperty()
	{
		static auto property = DependencyProperty::Register(L"Glass", xaml_typename<Windows::Foundation::IInspectable>(), xaml_typename<class_type>(), PropertyMetadata{ nullptr, PropertyChangedCallback{ Changed } });
		return property;
	}
	Windows::Foundation::IInspectable MaterialSwap::Glass() const
	{
		return GetValue(GlassProperty());
	}
	void MaterialSwap::Glass(Windows::Foundation::IInspectable const& value)
	{
		SetValue(GlassProperty(), value);
	}
	void MaterialSwap::Changed(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
	{
		get_self<MaterialSwap>(sender.as<class_type>())->UpdateContent();
	}
	void MaterialSwap::OnApplyTemplate()
	{
		__super::OnApplyTemplate();
		UpdateContent();
	}
	void MaterialSwap::UpdateContent()
	{
		if (ControlAppearance::IsShuttingDown() || !IsLoaded()) return;
		try
		{
			auto glass = Glass().try_as<FrameworkElement>();
			auto content = glass && ::OpenNet::UI::MaterialAppearance::IsGlass(glass) ? Glass() : Native();
			if (Content() != content) Content(content);
			::OpenNet::UI::EnsureMaterialHostBridge(*this);
		}
		catch (...)
		{
			LiquidGlassProfile::ReportCurrentFailure(L"switch wrapped material control");
		}
	}
	void MaterialSwap::Detach()
	{
		if (m_state) for (auto const& [property, token] : m_tokens) m_state.UnregisterPropertyChangedCallback(property, token);
		m_tokens.clear(); m_state = nullptr;
	}
	void MaterialSwap::LoadedHandler(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		if (ControlAppearance::IsShuttingDown()) return;
		Detach(); m_state = MaterialControlState::Current();
		for (auto const& property : { MaterialControlState::StyleIndexProperty(), MaterialControlState::SliderMaterialProperty(),
			MaterialControlState::SearchBoxMaterialProperty(), MaterialControlState::PasswordBoxMaterialProperty(), MaterialControlState::CheckBoxMaterialProperty() })
		{
			auto token = m_state.RegisterPropertyChangedCallback(property, [weak = get_weak()](auto const&, auto const&)
			{
				if (auto self = weak.get()) self->UpdateContent();
			});
			m_tokens.emplace_back(property, token);
		}
		UpdateContent();
	}
	void MaterialSwap::UnloadedHandler(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		if (ControlAppearance::IsShuttingDown() || IsLoaded()) return;
		Detach();
	}
}
