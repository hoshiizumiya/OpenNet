#include "XamlWorkaround.h"
#include "LiquidGlassProfile.h"
#include "MaterialControlState.h"
#include "ControlAppearance.h"
#if __has_include("UI/Xaml/Control/Theme/LiquidGlassProfile.g.cpp")
#include "UI/Xaml/Control/Theme/LiquidGlassProfile.g.cpp"
#endif
#if __has_include("UI/Xaml/Control/Theme/LiquidGlassParameter.g.cpp")
#include "UI/Xaml/Control/Theme/LiquidGlassParameter.g.cpp"
#endif
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Utils.Message;

using namespace winrt;
using namespace Microsoft::UI::Xaml;
namespace optics = ::OpenNet::UI::LiquidGlassOptics;

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    namespace
    {
        constexpr char Category[] = "liquid_glass_profiles_v1";
        Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile> s_profiles{ nullptr };
        std::string StorageKey(hstring const& kind, hstring const& parameter) { return to_string(kind) + "." + to_string(parameter); }
        bool IsEnabled(hstring const& kind)
        {
            auto state = MaterialControlState::Current();
            if (kind == L"Button") return state.ButtonEnabled();
            if (kind == L"ToggleButton") return state.ToggleButtonEnabled();
            if (kind == L"HyperlinkButton") return state.HyperlinkButtonEnabled();
            if (kind == L"CheckBox") return state.CheckBoxEnabled();
            if (kind == L"RadioButton") return state.RadioButtonEnabled();
            if (kind == L"Slider") return state.SliderEnabled();
            if (kind == L"TextBox") return state.TextBoxEnabled();
            if (kind == L"SearchBox") return state.SearchBoxEnabled();
            if (kind == L"PasswordBox") return state.PasswordBoxEnabled();
            if (kind == L"ComboBox") return state.ComboBoxEnabled();
            if (kind == L"ToggleSwitch") return state.ToggleSwitchEnabled();
            if (kind == L"Card") return state.CardEnabled();
            if (kind == L"FloatingPanel") return state.FloatingPanelEnabled();
            if (kind == L"TabBar") return state.TabBarEnabled();
            return false;
        }
        void SetEnabled(hstring const& kind, bool value)
        {
            auto state = MaterialControlState::Current();
            if (kind == L"Button") state.ButtonEnabled(value);
            if (kind == L"ToggleButton") state.ToggleButtonEnabled(value);
            if (kind == L"HyperlinkButton") state.HyperlinkButtonEnabled(value);
            if (kind == L"CheckBox") state.CheckBoxEnabled(value);
            if (kind == L"RadioButton") state.RadioButtonEnabled(value);
            if (kind == L"Slider") state.SliderEnabled(value);
            if (kind == L"TextBox") state.TextBoxEnabled(value);
            if (kind == L"SearchBox") state.SearchBoxEnabled(value);
            if (kind == L"PasswordBox") state.PasswordBoxEnabled(value);
            if (kind == L"ComboBox") state.ComboBoxEnabled(value);
            if (kind == L"ToggleSwitch") state.ToggleSwitchEnabled(value);
            if (kind == L"Card") state.CardEnabled(value);
            if (kind == L"FloatingPanel") state.FloatingPanelEnabled(value);
            if (kind == L"TabBar") state.TabBarEnabled(value);
        }
    }

    LiquidGlassParameter::LiquidGlassParameter(hstring const& kind, std::size_t index, double value)
        : m_kind(kind), m_index(index), m_default(value)
    {
        auto stored = ::OpenNet::Core::AppSettingsDatabase::Instance().GetDouble(Category, StorageKey(kind, Key()));
        m_overridden = stored && std::isfinite(*stored);
        Value(m_overridden ? *stored : value);
        m_loading = false;
    }
    hstring LiquidGlassParameter::Key() const { return optics::Descriptors[m_index].Key; }
    hstring LiquidGlassParameter::Label() const { return ResourceGetString((L"GlassParameter_" + Key()).c_str()); }
    double LiquidGlassParameter::Minimum() const { return std::min(optics::Descriptors[m_index].Minimum, m_default); }
    double LiquidGlassParameter::Maximum() const { return std::max(optics::Descriptors[m_index].Maximum, m_default); }
    double LiquidGlassParameter::Step() const { return optics::Descriptors[m_index].Step; }
    DependencyProperty LiquidGlassParameter::ValueProperty()
    {
        static auto property = DependencyProperty::Register(L"Value", xaml_typename<double>(), xaml_typename<class_type>(),
            PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ Changed } });
        return property;
    }
    double LiquidGlassParameter::Value() const { return unbox_value<double>(GetValue(ValueProperty())); }
    void LiquidGlassParameter::Value(double value)
    {
        SetValue(ValueProperty(), box_value(std::isfinite(value) ? std::clamp(value, Minimum(), Maximum()) : m_default));
    }
    void LiquidGlassParameter::Changed(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
    {
        auto self = get_self<LiquidGlassParameter>(sender.as<class_type>());
        if (self->m_loading) return;
        auto value = self->Value();
        self->m_overridden = std::abs(value - self->m_default) > 0.000001;
        auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
        auto key = StorageKey(self->m_kind, self->Key());
        if (self->m_overridden) db.SetDouble(Category, key, value);
        else db.Delete(Category, key);
        MaterialControlState::RefreshBrushes();
    }
    void LiquidGlassParameter::Reset()
    {
        m_loading = true;
        Value(m_default);
        m_overridden = false;
        ::OpenNet::Core::AppSettingsDatabase::Instance().Delete(Category, StorageKey(m_kind, Key()));
        m_loading = false;
        MaterialControlState::RefreshBrushes();
    }

    LiquidGlassProfile::LiquidGlassProfile(hstring const& key, hstring const&) : m_key(key), m_name(ResourceGetString((L"GlassKind_" + key).c_str()))
    {
        m_parameters = single_threaded_observable_vector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassParameter>();
        auto defaults = optics::Defaults(key);
        for (std::size_t i = 0; i < optics::Descriptors.size(); ++i)
            m_parameters.Append(make<LiquidGlassParameter>(key, i, optics::Descriptors[i].Read(defaults)));
        Enabled(IsEnabled(key));
        m_loading = false;
    }
    Windows::Foundation::Collections::IObservableVector<LiquidGlassProfile::class_type> LiquidGlassProfile::All()
    {
        if (!s_profiles)
        {
            auto profiles = single_threaded_observable_vector<class_type>();
            profiles.Append(make<LiquidGlassProfile>(L"Button", L"Buttons"));
            profiles.Append(make<LiquidGlassProfile>(L"ToggleButton", L"Toggle buttons"));
            profiles.Append(make<LiquidGlassProfile>(L"HyperlinkButton", L"Hyperlinks"));
            profiles.Append(make<LiquidGlassProfile>(L"CheckBox", L"Check boxes"));
            profiles.Append(make<LiquidGlassProfile>(L"RadioButton", L"Radio buttons"));
            profiles.Append(make<LiquidGlassProfile>(L"Slider", L"Sliders"));
            profiles.Append(make<LiquidGlassProfile>(L"TextBox", L"Text inputs"));
            profiles.Append(make<LiquidGlassProfile>(L"SearchBox", L"Search inputs"));
            profiles.Append(make<LiquidGlassProfile>(L"PasswordBox", L"Password inputs"));
            profiles.Append(make<LiquidGlassProfile>(L"ComboBox", L"Combo boxes"));
            profiles.Append(make<LiquidGlassProfile>(L"ToggleSwitch", L"Switches"));
            profiles.Append(make<LiquidGlassProfile>(L"Card", L"Cards and surfaces"));
            profiles.Append(make<LiquidGlassProfile>(L"FloatingPanel", L"Floating panels"));
            profiles.Append(make<LiquidGlassProfile>(L"TabBar", L"Tab bars"));
            s_profiles = profiles;
        }
        return s_profiles;
    }
    DependencyProperty LiquidGlassProfile::EnabledProperty()
    {
        static auto property = DependencyProperty::Register(L"Enabled", xaml_typename<bool>(), xaml_typename<class_type>(),
            PropertyMetadata{ box_value(true), PropertyChangedCallback{ Changed } });
        return property;
    }
    bool LiquidGlassProfile::Enabled() const { return unbox_value<bool>(GetValue(EnabledProperty())); }
    void LiquidGlassProfile::Enabled(bool value) { SetValue(EnabledProperty(), box_value(value)); }
    void LiquidGlassProfile::Changed(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
    {
        auto self = get_self<LiquidGlassProfile>(sender.as<class_type>());
        if (!self->m_loading) SetEnabled(self->Key(), self->Enabled());
    }
    void LiquidGlassProfile::SyncEnabled()
    {
        if (!s_profiles) return;
        for (auto const& profile : s_profiles)
        {
            auto self = get_self<LiquidGlassProfile>(profile);
            self->m_loading = true;
            profile.Enabled(IsEnabled(profile.Key()));
            self->m_loading = false;
        }
    }
    void LiquidGlassProfile::Reset()
    {
        // Reset only this type's optics. Its enable switch and other types are untouched.
        for (auto const& parameter : m_parameters) parameter.Reset();
    }
    void LiquidGlassProfile::Apply(hstring const& kind, WinUI::Composition::Hlsl::LiquidGlassBrush const& brush)
    {
        auto key = optics::Canonical(kind);
        for (auto const& profile : All())
        {
            if (profile.Key() != key) continue;
            auto parameters = profile.Parameters();
            for (std::uint32_t i = 0; i < parameters.Size(); ++i)
            {
                auto parameter = parameters.GetAt(i);
                if (get_self<LiquidGlassParameter>(parameter)->Overridden())
                    optics::Descriptors[i].Write(brush, parameter.Value());
            }
            return;
        }
    }
}
