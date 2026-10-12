#include "WindowsPlatform.h"
#include <CommCtrl.h>
#include "XamlWorkaround.h"
#include "ControlAppearance.h"
#include "MaterialControlState.h"
#include "Typography.h"
#include "LiquidGlassOptics.h"
#include "MaterialControlBridge.h"
#if __has_include("UI/Xaml/Control/Theme/ControlAppearance.g.cpp")
#include "UI/Xaml/Control/Theme/ControlAppearance.g.cpp"
#endif
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Xaml.Data;
import winrtplus.Microsoft.UI.Interop;
import OpenNet.XamlApplicationLifetime;

#pragma comment(lib, "Comctl32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    namespace
    {
        struct ControlEntry
        {
            weak_ref<Controls::Control> Control;
            Windows::Foundation::IInspectable OriginalTemplate{ nullptr };
            Data::Binding OriginalBinding{ nullptr };
            bool Active{};
            bool Loaded{};
            std::uint64_t WindowId{};
        };
        std::vector<ControlEntry> s_controls;
        bool s_refreshQueued{};
        bool s_stopping{};
        bool s_shutdownHooked{};
        bool s_hookFailureLogged{};

        struct ThemeObserver
        {
            std::uint64_t WindowId{};
            HWND Window{};
            bool HighContrast{};
        };
        std::vector<ThemeObserver> s_themeObservers;
        constexpr UINT_PTR ThemeSubclassId = 1;

        bool Stopping() noexcept
        {
            return s_stopping || ::OpenNet::XamlApplicationLifetime::Exiting.load();
        }

        bool HighContrast() noexcept
        {
            // Desktop-safe query: no CoreWindow or WinRT event registration required.
            HIGHCONTRASTW settings{};
            settings.cbSize = sizeof(settings);
            if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(settings), &settings, 0))
                return true; // Unknown accessibility state: retain native visuals.
            return (settings.dwFlags & HCF_HIGHCONTRASTON) != 0;
        }

        LRESULT CALLBACK ThemeWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR)
        {
            if (message == WM_NCDESTROY)
            {
                // No WinRT calls during window teardown. This record contains only
                // scalar data, so erasing it cannot revoke a late SDK subscription.
                RemoveWindowSubclass(window, &ThemeWindowProc, subclassId);
                std::erase_if(s_themeObservers, [&](auto const& item) { return item.Window == window; });
            }
            else if (!Stopping() && (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED || message == WM_SYSCOLORCHANGE))
            {
                auto found = std::find_if(s_themeObservers.begin(), s_themeObservers.end(), [&](auto const& item) { return item.Window == window; });
                if (found != s_themeObservers.end())
                {
                    auto contrast = HighContrast();
                    if (contrast != found->HighContrast)
                    {
                        found->HighContrast = contrast;
                        try
                        {
                            // Post only; do not replace XAML templates inside a WndProc.
                            ControlAppearance::Refresh();
                            MaterialControlState::RefreshBrushes();
                        }
                        catch (...)
                        {
                            // No exception may escape a Win32 callback. Template and
                            // brush work is dispatched separately and is not swallowed.
                            OutputDebugStringW(L"ControlAppearance: could not queue contrast refresh.\n");
                        }
                    }
                }
            }
            return DefSubclassProc(window, message, wParam, lParam);
        }

        std::uint64_t ObserveTheme(FrameworkElement const& control)
        {
            if (Stopping()) return 0;
            if (!s_shutdownHooked)
            {
                // The queue owns this handler. Deliberately no event_revoker that
                // would call back into the SDK after the queue has shut down.
                (void)control.DispatcherQueue().ShutdownStarting([](auto const&, auto const&)
                {
                    ControlAppearance::Shutdown();
                });
                s_shutdownHooked = true;
            }
            auto root = control.XamlRoot();
            if (!root) return 0;
            auto environment = root.ContentIslandEnvironment();
            if (!environment) return 0;
            auto windowId = environment.AppWindowId();
            if (!windowId.Value) return 0;
            auto found = std::find_if(s_themeObservers.begin(), s_themeObservers.end(), [&](auto const& item)
            {
                return item.WindowId == windowId.Value;
            });
            if (found != s_themeObservers.end()) return windowId.Value;

            auto window = Microsoft::UI::GetWindowFromWindowId(windowId);
            if (!window || GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId()) return 0;
            if (!SetWindowSubclass(window, &ThemeWindowProc, ThemeSubclassId, 0))
            {
                if (!s_hookFailureLogged)
                {
                    OutputDebugStringW(L"ControlAppearance: contrast window hook unavailable; retaining SPI query fallback.\n");
                    s_hookFailureLogged = true;
                }
                return 0;
            }
            // A successfully installed hook lives until WM_NCDESTROY/shutdown,
            // not until a control unloads (navigation and template changes unload controls).
            s_themeObservers.push_back({ windowId.Value, window, HighContrast() });
            return windowId.Value;
        }

        void RefreshSurface(FrameworkElement const& element)
        {
            if (Stopping()) return;
            auto brush = ControlAppearance::GetBrush(element).try_as<WinUI::Composition::Hlsl::LiquidGlassBrush>();
            if (!brush || !element.IsLoaded()) return;
            auto dark = element.ActualTheme() == ElementTheme::Dark;
            if (HighContrast())
            {
                auto color = GetSysColor(COLOR_WINDOW);
                brush.FallbackColor({ 255, GetRValue(color), GetGValue(color), GetBValue(color) });
                return;
            }
            auto kind = ControlAppearance::GetSurface(element);
            auto sliderThumb = kind == L"Slider";
            auto switchThumb = kind == L"ToggleSwitch" || kind == L"ToggleSwitchOff";
            if (sliderThumb || switchThumb)
            {
                // Knobs are foreground affordances, not translucent dark panels.
                // Keep a light body over an accent track; an off switch in Light
                // theme needs a dark body instead, like the native control.
                auto lightBody = kind != L"ToggleSwitchOff" || dark;
                auto channel = lightBody ? 1.0 : 28.0 / 255.0;
                brush.TintRed(channel); brush.TintGreen(channel); brush.TintBlue(channel);
                brush.TintOpacity(.9);
                brush.FallbackColor(lightBody ? Windows::UI::Color{ 255, 255, 255, 255 } : Windows::UI::Color{ 255, 28, 28, 28 });
                auto extent = std::min(element.ActualWidth(), element.ActualHeight());
                if (extent > 0.0)
                {
                    // Scale the preset's authored 60/92-DIP lens to native geometry.
                    // Use constants, not current brush values: SizeChanged must not
                    // compound scaling or reset the user's strength multipliers.
                    auto scale = extent / (sliderThumb ? 60.0 : 92.0);
                    brush.CornerRadius(extent * .5);
                    brush.BezelWidth((sliderThumb ? 16.0 : 19.0) * scale);
                    brush.GlassThickness((sliderThumb ? 80.0 : 47.0) * scale);
                }
                return;
            }
            if (kind == L"Button" || kind == L"ToggleButton" || kind == L"HyperlinkButton")
            {
                // Preserve opaque accent/custom backgrounds so on-accent text keeps its contrast.
                auto parent = Media::VisualTreeHelper::GetParent(element);
                while (parent)
                {
                    if (auto owner = parent.try_as<Controls::Control>())
                    {
                        if (auto fill = owner.Background().try_as<Media::SolidColorBrush>(); fill && fill.Color().A >= 192)
                        {
                            auto color = fill.Color();
                            brush.TintRed(color.R / 255.0); brush.TintGreen(color.G / 255.0); brush.TintBlue(color.B / 255.0);
                            brush.TintOpacity(.8);
                        }
                        break;
                    }
                    parent = Media::VisualTreeHelper::GetParent(parent);
                }
            }
            if (kind == L"Card" || kind == L"FloatingPanel" || kind == L"TabBar")
            {
                // Geometry follows the app's native surface, not Panel's 31-DIP radius.
                double radius = 4.0;
                if (auto border = element.try_as<Controls::Border>()) radius = border.CornerRadius().TopLeft;
                else if (auto control = element.try_as<Controls::Control>()) radius = control.CornerRadius().TopLeft;
                brush.CornerRadius(radius);
                auto extent = std::min(element.ActualWidth(), element.ActualHeight());
                if (extent > 0.0) brush.BezelWidth(std::min(29.0, extent * .2));
                brush.FallbackColor(dark ? Windows::UI::Color{ 255, 40, 40, 40 } : Windows::UI::Color{ 255, 251, 251, 251 });
            }
            if (kind == L"RadioButton")
            {
                auto radius = std::min(element.ActualWidth(), element.ActualHeight()) * .5;
                if (radius > 0.0) { brush.CornerRadius(radius); brush.BezelWidth(std::min(8.0, radius * .5)); }
            }
            else if (kind != L"Card" && kind != L"FloatingPanel" && kind != L"TabBar")
            {
                if (auto border = element.try_as<Controls::Border>()) brush.CornerRadius(border.CornerRadius().TopLeft);
                else if (auto presenter = element.try_as<Controls::ContentPresenter>()) brush.CornerRadius(presenter.CornerRadius().TopLeft);
                auto extent = std::min(element.ActualWidth(), element.ActualHeight());
                if (extent > 0.0) brush.BezelWidth(std::min(brush.BezelWidth(), extent * .25));
            }
        }

        DependencyProperty HookedProperty()
        {
            static auto property = DependencyProperty::RegisterAttached(L"Hooked", xaml_typename<bool>(),
                xaml_typename<winrt::OpenNet::UI::Xaml::Control::Theme::ControlAppearance>(), PropertyMetadata{ box_value(false) });
            return property;
        }

        std::pair<hstring, bool> Selection(Controls::Control const& control)
        {
            auto state = MaterialControlState::Current();
            if (control.try_as<Controls::AutoSuggestBox>()) return { L"AutoSuggestBox", state.SearchBoxMaterial() == 1 };
            if (control.try_as<Controls::ToggleSwitch>()) return { L"ToggleSwitch", state.ToggleSwitchMaterial() == 1 };
            if (control.try_as<Controls::CheckBox>()) return { L"CheckBox", state.CheckBoxMaterial() == 1 };
            if (control.try_as<Controls::RadioButton>()) return { L"RadioButton", state.RadioButtonMaterial() == 1 };
            if (control.try_as<Controls::Primitives::ToggleButton>()) return { L"ToggleButton", state.ToggleButtonMaterial() == 1 };
            if (control.try_as<Controls::HyperlinkButton>()) return { L"HyperlinkButton", state.HyperlinkButtonMaterial() == 1 };
            if (control.try_as<Controls::Button>()) return { L"Button", state.ButtonMaterial() == 1 };
            if (control.try_as<Controls::Slider>()) return { L"Slider", state.SliderMaterial() == 1 };
            if (control.try_as<Controls::TextBox>()) return { L"TextBox", state.TextBoxMaterial() == 1 };
            if (control.try_as<Controls::PasswordBox>()) return { L"PasswordBox", state.PasswordBoxMaterial() == 1 };
            if (control.try_as<Controls::ComboBox>()) return { L"ComboBox", state.ComboBoxMaterial() == 1 };
            if (control.try_as<Controls::ContentControl>()) return { L"Card", state.CardMaterial() == 1 };
            return { L"", false };
        }

        bool HasButtonSurface(Controls::Control const& control, hstring const& kind)
        {
            if (kind != L"Button" && kind != L"ToggleButton" && kind != L"HyperlinkButton") return true;
            // Transparent/subtle buttons deliberately have no resting surface.
            // Retain their complete native template, including hover/pressed states,
            // rather than turn every table header or inline link into a glass card.
            auto background = control.Background();
            if (!background || background.Opacity() == 0.0) return false;
            if (auto solid = background.try_as<Media::SolidColorBrush>()) return solid.Color().A != 0;
            return true;
        }

        ResourceDictionary Templates()
        {
            // Compiled templates are shared. Surface brushes are NEVER dictionary resources.
            static ResourceDictionary templates{ nullptr };
            if (!templates)
            {
                ResourceDictionary candidate;
                candidate.Source(Windows::Foundation::Uri{ L"ms-appx:///UI/Xaml/Control/Theme/MaterialLibraryTemplates.xaml" });
                templates = candidate;
            }
            return templates;
        }

        void Apply(ControlEntry& entry)
        {
            auto control = entry.Control.get();
            if (Stopping() || !control || !control.IsLoaded()) return;
            auto [kind, active] = Selection(control);
            active = (active || ControlAppearance::GetIsPreview(control)) && ControlAppearance::GetIsEnabled(control);
            active = active && HasButtonSurface(control, kind);
            // Keep the platform's accessible high-contrast visuals intact.
            active = active && !HighContrast();
            if (active == entry.Active) return;
            if (active)
            {
                auto key = box_value(L"Material" + kind + L"Template");
                auto templates = Templates();
                if (!templates.HasKey(key)) return;
                entry.OriginalTemplate = control.ReadLocalValue(Controls::Control::TemplateProperty());
                if (auto expression = control.GetBindingExpression(Controls::Control::TemplateProperty()))
                    entry.OriginalBinding = expression.ParentBinding();
                entry.Active = true;
                control.Template(templates.Lookup(key).as<Controls::ControlTemplate>());
                control.ApplyTemplate();
                ::OpenNet::UI::EnsureMaterialTemplateBridge(control);
            }
            else
            {
                entry.Active = false;
                ::OpenNet::UI::ReleaseMaterialTemplateBridge(control);
                if (entry.OriginalBinding)
                    control.SetBinding(Controls::Control::TemplateProperty(), entry.OriginalBinding);
                else if (entry.OriginalTemplate == DependencyProperty::UnsetValue())
                    control.ClearValue(Controls::Control::TemplateProperty());
                else
                    control.SetValue(Controls::Control::TemplateProperty(), entry.OriginalTemplate);
                entry.OriginalTemplate = nullptr;
                entry.OriginalBinding = nullptr;
            }
        }
    }

    void ControlAppearance::EnsureProperties()
    {
        (void)IsEnabledProperty();
        (void)SurfaceProperty();
        (void)BrushProperty();
        (void)IsPreviewProperty();
        (void)IsMaterialHostProperty();
    }

    DependencyProperty ControlAppearance::IsMaterialHostProperty()
    {
        static auto property = DependencyProperty::RegisterAttached(L"IsMaterialHost", xaml_typename<bool>(), xaml_typename<class_type>(),
            PropertyMetadata{ box_value(false), PropertyChangedCallback{ [](DependencyObject const& object, auto const&)
            {
                if (auto element = object.try_as<FrameworkElement>())
                    element.Loaded([](auto const& sender, auto const&) { ::OpenNet::UI::EnsureMaterialHostBridge(sender.template as<DependencyObject>()); });
            } } });
        return property;
    }
    bool ControlAppearance::GetIsMaterialHost(DependencyObject const& target) { return unbox_value<bool>(target.GetValue(IsMaterialHostProperty())); }
    void ControlAppearance::SetIsMaterialHost(DependencyObject const& target, bool value) { target.SetValue(IsMaterialHostProperty(), box_value(value)); }

    void ControlAppearance::AdaptSurface(FrameworkElement const& element) { RefreshSurface(element); }
    bool ControlAppearance::CanUseGlass() { return !Stopping() && !HighContrast(); }

    DependencyProperty ControlAppearance::IsPreviewProperty()
    {
        static auto property = DependencyProperty::RegisterAttached(L"IsPreview", xaml_typename<bool>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(false) });
        return property;
    }
    bool ControlAppearance::GetIsPreview(DependencyObject const& target) { return unbox_value<bool>(target.GetValue(IsPreviewProperty())); }
    void ControlAppearance::SetIsPreview(DependencyObject const& target, bool value) { target.SetValue(IsPreviewProperty(), box_value(value)); }

    void ControlAppearance::Shutdown() noexcept
    {
        // UI-thread call before Window.Close/Application.Exit; also safe from
        // DispatcherQueue.ShutdownStarting because cleanup uses only Win32 APIs.
        s_stopping = true;
        s_refreshQueued = false;
        for (auto const& observer : s_themeObservers)
        {
            if (observer.Window && IsWindow(observer.Window))
                RemoveWindowSubclass(observer.Window, &ThemeWindowProc, ThemeSubclassId);
        }
        s_themeObservers.clear();
    }

    DependencyProperty ControlAppearance::IsEnabledProperty()
    {
        static auto property = DependencyProperty::RegisterAttached(L"IsEnabled", xaml_typename<bool>(), xaml_typename<class_type>(),
            PropertyMetadata{ box_value(false), PropertyChangedCallback{ EnabledChanged } });
        return property;
    }
    bool ControlAppearance::GetIsEnabled(DependencyObject const& target)
    {
        return unbox_value<bool>(target.GetValue(IsEnabledProperty()));
    }
    void ControlAppearance::SetIsEnabled(DependencyObject const& target, bool value)
    {
        target.SetValue(IsEnabledProperty(), box_value(value));
    }
    DependencyProperty ControlAppearance::SurfaceProperty()
    {
        static auto property = DependencyProperty::RegisterAttached(L"Surface", xaml_typename<hstring>(), xaml_typename<class_type>(),
            PropertyMetadata{ box_value(L""), PropertyChangedCallback{ SurfaceChanged } });
        return property;
    }
    hstring ControlAppearance::GetSurface(DependencyObject const& target)
    {
        return unbox_value<hstring>(target.GetValue(SurfaceProperty()));
    }
    void ControlAppearance::SetSurface(DependencyObject const& target, hstring const& value)
    {
        target.SetValue(SurfaceProperty(), box_value(value));
    }
    DependencyProperty ControlAppearance::BrushProperty()
    {
        static auto property = DependencyProperty::RegisterAttached(L"Brush", xaml_typename<Media::Brush>(), xaml_typename<class_type>(),
            PropertyMetadata{ nullptr });
        return property;
    }
    Media::Brush ControlAppearance::GetBrush(DependencyObject const& target)
    {
        return target.GetValue(BrushProperty()).try_as<Media::Brush>();
    }
    void ControlAppearance::SetBrush(DependencyObject const& target, Media::Brush const& value)
    {
        target.SetValue(BrushProperty(), value);
    }

    void ControlAppearance::EnabledChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
    {
        auto control = sender.try_as<Controls::Control>();
        if (!control) return;
        if (!unbox_value<bool>(control.GetValue(HookedProperty())))
        {
            control.Loaded(&Loaded);
            control.Unloaded(&Unloaded);
            // Theme/style changes can turn a filled button into a transparent one
            // (or vice versa); re-evaluate eligibility without a new control instance.
            (void)control.RegisterPropertyChangedCallback(Controls::Control::BackgroundProperty(), [](auto const&, auto const&) { Refresh(); });
            control.ActualThemeChanged([](auto const&, auto const&) { Refresh(); });
            control.SetValue(HookedProperty(), box_value(true));
        }
        // A dependency-property callback is also invoked by the XAML parser.
        // Do not activate services or apply templates from that callback.
        if (control.IsLoaded())
        {
            control.DispatcherQueue().TryEnqueue([weak = make_weak(control)]
            {
                if (auto current = weak.get(); current && current.IsLoaded()) Loaded(current, RoutedEventArgs{});
            });
        }
    }
    void ControlAppearance::Loaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
    {
        if (Stopping()) return;
        auto control = sender.as<Controls::Control>();
        auto entry = std::find_if(s_controls.begin(), s_controls.end(), [&](auto const& item) { return item.Control.get() == control; });
        if (entry == s_controls.end())
        {
            s_controls.push_back({ make_weak(control) });
            entry = std::prev(s_controls.end());
        }
        entry->Loaded = true;
        Typography::Track(control, control.FontSize());
        Refresh();
    }
    void ControlAppearance::Unloaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
    {
        if (Stopping()) return;
        auto control = sender.as<Controls::Control>();
        auto entry = std::find_if(s_controls.begin(), s_controls.end(), [&](auto const& item) { return item.Control.get() == control; });
        if (entry != s_controls.end()) entry->Loaded = false;
        // Keep the weak entry and original template across cached-page unload/reload.
        std::erase_if(s_controls, [](auto const& item) { return !item.Control.get(); });
    }
    void ControlAppearance::Refresh()
    {
        if (Stopping() || s_refreshQueued) return;
        std::erase_if(s_controls, [](auto const& item) { return !item.Control.get(); });
        if (s_controls.empty()) return;
        auto control = s_controls.front().Control.get();
        s_refreshQueued = control.DispatcherQueue().TryEnqueue([]
        {
            s_refreshQueued = false;
            if (Stopping()) return;
            std::erase_if(s_controls, [](auto const& item) { return !item.Control.get(); });
            // Template changes can raise Loaded/Unloaded; iterate a stable snapshot.
            auto snapshot = s_controls;
            for (auto& item : snapshot)
            {
                auto control = item.Control.get();
                if (!control || !item.Loaded || !control.IsLoaded()) continue;
                auto windowId = Selection(control).second && GetIsEnabled(control) ? ObserveTheme(control) : 0;
                auto owner = std::find_if(s_controls.begin(), s_controls.end(), [&](auto const& current) { return current.Control.get() == control; });
                if (owner != s_controls.end()) owner->WindowId = windowId;
                Apply(item);
                auto found = std::find_if(s_controls.begin(), s_controls.end(), [&](auto const& current) { return current.Control.get() == control; });
                if (found != s_controls.end())
                {
                    // Preserve lifecycle state updated by reentrant child load/unload callbacks.
                    found->Active = item.Active;
                    found->OriginalTemplate = item.OriginalTemplate;
                    found->OriginalBinding = item.OriginalBinding;
                    found->WindowId = windowId;
                }
            }
        });
    }
    void ControlAppearance::SurfaceChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const& args)
    {
        auto element = sender.try_as<FrameworkElement>();
        if (!element || unbox_value<hstring>(args.OldValue()) != L"") return;
        element.Loaded(&SurfaceLoaded);
        element.ActualThemeChanged([](auto const&, auto const&) { MaterialControlState::RefreshBrushes(); });
        element.SizeChanged([](auto const&, auto const&) { MaterialControlState::RefreshBrushes(); });
        if (element.IsLoaded()) SurfaceLoaded(element, RoutedEventArgs{});
    }
    void ControlAppearance::SurfaceLoaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
    {
        if (Stopping()) return;
        auto element = sender.as<FrameworkElement>();
        (void)ObserveTheme(element);
        // AutoSuggestBox's inner TextBox shares the search category, not the
        // independent TextBox category. Keep its native input template/behavior.
        if (GetSurface(element) == L"TextBox")
        {
            auto ancestor = Media::VisualTreeHelper::GetParent(element);
            while (ancestor)
            {
                if (ancestor.try_as<Controls::AutoSuggestBox>()) { SetSurface(element, L"AutoSuggestBox"); break; }
                ancestor = Media::VisualTreeHelper::GetParent(ancestor);
            }
        }
        if (!GetBrush(element))
        {
            auto kind = GetSurface(element);
            auto brush = ::OpenNet::UI::LiquidGlassOptics::Defaults(::OpenNet::UI::LiquidGlassOptics::Canonical(kind));
            // Native slider/switch geometry is compact; do not reuse Kube's authored dimensions.
            auto radius = std::min(element.ActualWidth(), element.ActualHeight()) * .5;
            if (kind == L"Slider" || kind == L"ToggleSwitch" || kind == L"ToggleSwitchOff" || kind == L"RadioButton") brush.CornerRadius(radius);
            else if (kind == L"CheckBox") brush.CornerRadius(4.0);
            SetBrush(element, brush);
        }
        auto parent = Media::VisualTreeHelper::GetParent(element);
        while (parent)
        {
            if (GetIsPreview(parent)) { SetIsPreview(element, true); break; }
            parent = Media::VisualTreeHelper::GetParent(parent);
        }
        MaterialControlState::SetIsManaged(element, true);
    }
}
