#include "XamlWorkaround.h"
#include "LiquidGlassControlsWindow.xaml.h"
#include "LiquidGlassPreview.h"
#include "UI/Xaml/Control/Theme/LiquidGlassProfile.h"
#if __has_include("UI/Xaml/View/Windows/LiquidGlassControlsWindow.g.cpp")
#include "UI/Xaml/View/Windows/LiquidGlassControlsWindow.g.cpp"
#endif
import OpenNet.Helpers.MaterialTheme;
using namespace winrt;
using namespace Microsoft::UI::Xaml;
namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
    LiquidGlassControlsWindow::LiquidGlassControlsWindow()
    {
        m_profile = Profiles().GetAt(0);
        InitializeComponent();
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(WindowTitleBar());
        InitializeWindowExBase();
        MaterialStyleSelector().SelectedIndex(static_cast<int>(::OpenNet::Helpers::MaterialTheme::Current()));
        TypeSelector().SelectedIndex(0);
        PreviewHost().Child(::OpenNet::UI::GlassPreview(m_profile.Key(), false));
        m_loading = false;
    }
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile> LiquidGlassControlsWindow::Profiles()
    {
        return winrt::OpenNet::UI::Xaml::Control::Theme::implementation::LiquidGlassProfile::All();
    }
    void LiquidGlassControlsWindow::TypeChanged(winrt::Windows::Foundation::IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loading) return;
        auto profile = TypeSelector().SelectedItem().try_as<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile>();
        if (!profile) return;
        m_profile = profile;
        PreviewHost().Child(::OpenNet::UI::GlassPreview(profile.Key(), false));
        Bindings->Update();
    }
    void LiquidGlassControlsWindow::ResetClicked(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        for (auto const& profile : Profiles()) profile.Enabled(true);
    }
    void LiquidGlassControlsWindow::StyleChanged(winrt::Windows::Foundation::IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loading) return;
        auto index = MaterialStyleSelector().SelectedIndex();
        if (index >= 0 && index <= 1) ::OpenNet::Helpers::MaterialTheme::Set(static_cast<::OpenNet::Helpers::MaterialStyle>(index));
    }
}
