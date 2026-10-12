#include "XamlWorkaround.h"
#include "LiquidGlassEffectsWindow.xaml.h"
#include "LiquidGlassPreview.h"
#include "UI/Xaml/Control/Theme/LiquidGlassProfile.h"
#if __has_include("UI/Xaml/View/Windows/LiquidGlassEffectsWindow.g.cpp")
#include "UI/Xaml/View/Windows/LiquidGlassEffectsWindow.g.cpp"
#endif
import OpenNet.Helpers.MaterialTheme;
using namespace winrt;
using namespace Microsoft::UI::Xaml;
namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
    LiquidGlassEffectsWindow::LiquidGlassEffectsWindow()
    {
        m_profile = Profiles().GetAt(0);
        InitializeComponent();
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(WindowTitleBar());
        InitializeWindowExBase();
        
        TypeSelector().SelectedIndex(0);
        PreviewHost().Child(::OpenNet::UI::GlassPreview(m_profile.Key(), true));
        m_loading = false;
    }
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile> LiquidGlassEffectsWindow::Profiles()
    {
        return winrt::OpenNet::UI::Xaml::Control::Theme::implementation::LiquidGlassProfile::All();
    }
    void LiquidGlassEffectsWindow::TypeChanged(winrt::Windows::Foundation::IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loading) return;
        auto profile = TypeSelector().SelectedItem().try_as<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile>();
        if (!profile) return;
        m_profile = profile;
        PreviewHost().Child(::OpenNet::UI::GlassPreview(profile.Key(), true));
        Bindings->Update();
    }
    void LiquidGlassEffectsWindow::ResetClicked(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        if (m_profile) m_profile.Reset();
    }

}
