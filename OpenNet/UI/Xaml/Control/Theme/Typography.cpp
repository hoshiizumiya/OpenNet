#include "XamlWorkaround.h"
#include "Typography.h"
#if __has_include("UI/Xaml/Control/Theme/Typography.g.cpp")
#include "UI/Xaml/Control/Theme/Typography.g.cpp"
#endif
import OpenNet.Core.AppSettingsDatabase;

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    namespace
    {
        struct TextEntry { weak_ref<FrameworkElement> Element; double BaseSize; };
        std::vector<TextEntry> s_text;

        DependencyProperty FontProperty(FrameworkElement const& element)
        {
            if (element.try_as<Controls::TextBlock>()) return Controls::TextBlock::FontSizeProperty();
            if (element.try_as<Controls::RichTextBlock>()) return Controls::RichTextBlock::FontSizeProperty();
            return Controls::Control::FontSizeProperty();
        }
        void Apply(TextEntry const& entry, double scale)
        {
            auto element = entry.Element.get();
            if (!element || !element.IsLoaded()) return;
            element.SetValue(FontProperty(element), box_value(entry.BaseSize * scale));
        }
    }
    Typography::Typography()
    {
        EnsureProperties();
        auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
        database.Initialize();
        Scale(database.GetDouble("typography", "scale").value_or(1.0));
    }
    void Typography::EnsureProperties()
    {
        // Optimized Style.Setters resolve native DPs without invoking Get/Set accessors.
        // C++ XamlUserType.RunInitializer does not invoke function-local static getters.
        (void)ScaleProperty();
        (void)BaseFontSizeProperty();
    }
    winrt::OpenNet::UI::Xaml::Control::Theme::Typography Typography::Current()
    {
        static auto state = make<Typography>();
        return state;
    }
    DependencyProperty Typography::ScaleProperty()
    {
        static auto property = DependencyProperty::Register(L"Scale", xaml_typename<double>(), xaml_typename<class_type>(),
            PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ ScaleChanged } });
        return property;
    }
    double Typography::Scale() { return unbox_value<double>(GetValue(ScaleProperty())); }
    void Typography::Scale(double value)
    {
        SetValue(ScaleProperty(), box_value(std::isfinite(value) ? std::clamp(value, .8, 2.0) : 1.0));
    }
    hstring Typography::FormatScale(double value) { return hstring(std::format(L"{:.0f}%", value * 100.0)); }
    DependencyProperty Typography::BaseFontSizeProperty()
    {
        static auto property = DependencyProperty::RegisterAttached(L"BaseFontSize", xaml_typename<double>(), xaml_typename<class_type>(),
            PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ BaseSizeChanged } });
        return property;
    }
    double Typography::GetBaseFontSize(DependencyObject const& target) { return unbox_value<double>(target.GetValue(BaseFontSizeProperty())); }
    void Typography::SetBaseFontSize(DependencyObject const& target, double value) { target.SetValue(BaseFontSizeProperty(), box_value(value)); }
    void Typography::ScaleChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
    {
        auto scale = sender.as<class_type>().Scale();
        ::OpenNet::Core::AppSettingsDatabase::Instance().SetDouble("typography", "scale", scale);
        std::erase_if(s_text, [](auto const& item) { return !item.Element.get(); });
        for (auto const& item : s_text) Apply(item, scale);
    }
    void Typography::BaseSizeChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const& args)
    {
        auto element = sender.try_as<FrameworkElement>();
        if (!element) return;
        if (unbox_value<double>(args.OldValue()) == 0.0) element.Loaded(&Loaded);
        if (element.IsLoaded()) Loaded(element, RoutedEventArgs{});
    }
    void Typography::Loaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
    {
        auto element = sender.as<FrameworkElement>();
        Track(element, GetBaseFontSize(element));
    }
    void Typography::Track(FrameworkElement const& element, double baseSize)
    {
        if (!(baseSize > 0.0)) return;
        auto found = std::find_if(s_text.begin(), s_text.end(), [&](auto const& item) { return item.Element.get() == element; });
        if (found == s_text.end())
        {
            // A page's explicit font size wins over the default type ramp.
            auto local = element.ReadLocalValue(FontProperty(element));
            if (auto value = local.try_as<Windows::Foundation::IReference<double>>()) baseSize = value.Value();
            s_text.push_back({ make_weak(element), baseSize });
            found = std::prev(s_text.end());
        }
        Apply(*found, Current().Scale());
    }
}
