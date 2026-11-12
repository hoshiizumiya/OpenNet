#pragma once
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.WinUI.LiquidGlass;

namespace OpenNet::UI
{
	inline bool IsMaterialSwitch(winrt::Windows::Foundation::IInspectable const& sender)
	{
		return sender && (sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch>() || sender.try_as<winrt::WinUI::LiquidGlass::LiquidGlassToggleSwitch>());
	}
	inline bool ReadMaterialSwitch(winrt::Windows::Foundation::IInspectable const& sender)
	{
		if (auto glass = sender.try_as<winrt::WinUI::LiquidGlass::LiquidGlassToggleSwitch>())
		{
			auto checked = glass.IsChecked();
			return checked && checked.Value();
		}
		if (auto native = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch>()) return native.IsOn();
		return false;
	}
	inline void PrepareMaterialSwitchEvent(winrt::Windows::Foundation::IInspectable const& sender)
	{
		// Checked/Unchecked can run before the library's IsChecked DP callback.
		// Publish the current checked state before existing handlers read IsOn.
		if (auto glass = sender.try_as<winrt::WinUI::LiquidGlass::LiquidGlassToggleSwitch>())
			glass.IsOn(ReadMaterialSwitch(sender));
	}
}
