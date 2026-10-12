#pragma once
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
namespace OpenNet::UI
{
    void EnsureMaterialHostBridge(winrt::Microsoft::UI::Xaml::DependencyObject const& sender);
    void EnsureMaterialTemplateBridge(winrt::Microsoft::UI::Xaml::Controls::Control const& native);
    void ReleaseMaterialTemplateBridge(winrt::Microsoft::UI::Xaml::Controls::Control const& native);
}
