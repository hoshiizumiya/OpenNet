# WinUI 3 Custom Controls in C++/WinRT: Practical Guide

[简体中文](./xaml-custom-controls-guide.zh-CN.md)

This guide uses OpenNet's TitleCard as the primary example for implementing reusable controls with a C++/WinRT runtimeclass, DependencyProperty values, and a XAML ControlTemplate. It focuses on the repository's actual file layout, build-generation chain, and debugging workflow.

## 1. First decision: templated Control or XAML-backed control?

TitleCard is a **templated Control**:

- The type is defined by IDL and implemented in C++.
- Its visuals live in a Style / ControlTemplate inside a ResourceDictionary.
- It does not need TitleCard.xaml, TitleCard.xaml.h, or TitleCard.xaml.cpp.

Current files:

- OpenNet/UI/Xaml/Control/Card/TitleCard.idl
- OpenNet/UI/Xaml/Control/Card/TitleCard.h
- OpenNet/UI/Xaml/Control/Card/TitleCard.cpp
- OpenNet/UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml

Why this distinction matters: adding a same-named TitleCard.xaml to a pure templated Control introduces a separate XAML code-behind generation path. That increases the chance of conflicts between Generated Files, XamlTypeInfo, and runtimeclass registration.

If the component genuinely owns a XAML tree and code-behind lifecycle, use an appropriate XAML-backed type such as UserControl. Do not mix both models merely to reuse a style.

## 2. Recommended implementation order

When adding a new control, use this order:

1. Define the IDL runtimeclass.
2. Make sure idl / h / cpp are included correctly by the project.
3. Implement the constructor and the minimum DependencyProperty set.
4. Confirm that C++/WinRT generated code compiles.
5. Add the ResourceDictionary and ControlTemplate.
6. Connect the dictionary through App.xaml or another resource entry point.
7. Start the full application and instantiate the control.

Why: IDL/generated-code failures and XAML-template failures occur in different stages. Adding everything at once makes an XamlTypeInfo.g.cpp failure much harder to localize.

## 3. IDL: expose only the public WinRT surface

TitleCard currently declares:

~~~idl
runtimeclass TitleCard : Microsoft.UI.Xaml.Controls.Control
{
    TitleCard();

    String Title;
    Object TitleContent;
    Object Content;
    Microsoft.UI.Xaml.Visibility DividerVisibility;
}
~~~

IDL produces the WinRT metadata and C++/WinRT generated declarations that make the custom type visible to XAML.

Practical rules:

- Put only members that genuinely need WinRT/XAML visibility in IDL.
- Keep internal helpers, caches, and template-part details in C++.
- After changing IDL, inspect Generated Files first if later compilation fails.

## 4. DependencyProperty: template-facing, styleable properties should use the property system

TitleCard stores Title, TitleContent, Content, and DividerVisibility as dependency properties.

Title is registered like this:

~~~cpp
Microsoft::UI::Xaml::DependencyProperty TitleCard::TitleProperty()
{
    static Microsoft::UI::Xaml::DependencyProperty s_property =
        Microsoft::UI::Xaml::DependencyProperty::Register(
            L"Title",
            winrt::xaml_typename<hstring>(),
            winrt::xaml_typename<OpenNet::UI::Xaml::Control::Card::TitleCard>(),
            Microsoft::UI::Xaml::PropertyMetadata{
                nullptr,
                Microsoft::UI::Xaml::PropertyChangedCallback{
                    &TitleCard::OnVisualPropertyChanged } });

    return s_property;
}
~~~

The function-local static is important: Register runs once and subsequent calls return the same DP.

A plain C++ field is not a substitute when the value participates in ControlTemplate, Style, Binding, animation, or dependency-property precedence.

### 4.1 Keep property accessors as property-system accessors

~~~cpp
hstring TitleCard::Title()
{
    return winrt::unbox_value_or<hstring>(GetValue(TitleProperty()), L"");
}

void TitleCard::Title(hstring const& value)
{
    SetValue(TitleProperty(), winrt::box_value(value));
}
~~~

Avoid maintaining a second independent C++ backing state for the same public property; otherwise the C++ value and XAML property-system value can diverge.

## 5. DefaultStyleKey: tell WinUI which default style to resolve

The constructor currently does:

~~~cpp
TitleCard::TitleCard()
{
    DefaultStyleKey(winrt::box_value(winrt::xaml_typename<class_type>()));
}
~~~

Without the correct DefaultStyleKey, the control may not resolve its default template even when an implicit Style targeting TitleCard exists.

## 6. ResourceDictionary: how OpenNet actually wires TitleCard today

The template is stored in:

- OpenNet/UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml

It is currently merged by **OpenNet/App.xaml**:

~~~xaml
<ResourceDictionary
    Source="ms-appx:///UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml" />
~~~

The previous guide described Generic.xaml as the current integration point; that was inaccurate. Generic.xaml is a common default-style entry point for control libraries, but **OpenNet currently loads TitleCard through App.xaml**.

For a new control, prefer consistency with the repository's current resource organization. Moving styles to Generic.xaml should be treated as a separate resource-architecture change rather than mixed into an unrelated control addition.

## 7. TemplateBinding: when it is the right tool

Typical TitleCard template forwarding:

~~~xaml
<TextBlock Text="{TemplateBinding Title}" />

<ContentPresenter
    Content="{TemplateBinding TitleContent}" />

<ContentPresenter
    Content="{TemplateBinding Content}" />
~~~

TemplateBinding is appropriate for directly forwarding dependency-property values from the templated parent.

Use Binding + RelativeSource=TemplatedParent when you need the full Binding feature set such as converters, complex paths, or fallback behavior.

A C++ getter returning a value is not enough for template-system participation. The template needs the corresponding dependency-property metadata.

## 8. VisualState: derived visual state should not silently overwrite caller input

TitleCard calls SetVisualStates after applying its template:

~~~cpp
void TitleCard::OnApplyTemplate()
{
    TitleCardT<TitleCard>::OnApplyTemplate();
    SetVisualStates();
}
~~~

The template contains:

~~~xaml
<VisualState x:Name="TitleGridVisible" />
<VisualState x:Name="TitleGridCollapsed">
    <VisualState.Setters>
        <Setter Target="TitleGrid.Visibility" Value="Collapsed" />
    </VisualState.Setters>
</VisualState>
~~~

That is the preferred direction: **VisualState should normally mutate template parts instead of writing derived state back into caller-facing dependency properties.**

The current TitleCard::SetVisualStates also assigns DividerVisibility directly. That can override a value explicitly provided by the caller. A future refactor should prefer:

- caller input in the public DP;
- derived internal state expressed through VisualState setters on template elements;
- a separate internal/read-only state if both concepts must coexist.

This prevents the common “I set the property, but the control changed it back internally” failure mode.

## 9. Project items: do not turn normal h/cpp into fake XAML code-behind

OpenNet.vcxproj currently groups TitleCard.cpp under TitleCard.idl:

~~~xml
<ClCompile Include="UI\Xaml\Control\Card\TitleCard.cpp">
  <DependentUpon>UI\Xaml\Control\Card\TitleCard.idl</DependentUpon>
</ClCompile>
~~~

That is a project-organization relationship.

What must be avoided is configuring ordinary TitleCard.h / TitleCard.cpp as code-behind for a nonexistent TitleCard.xaml. That can make the XAML compiler generate TitleCard.xaml.g.h / .g.cpp unexpectedly.

If Generated Files suddenly contains same-named XAML artifacts, inspect vcxproj Page, ApplicationDefinition, DependentUpon, and naming relationships first.

## 10. g.h / g.cpp: include the generated files for the actual runtimeclass

The header uses:

~~~cpp
#include "UI/Xaml/Control/Card/TitleCard.g.h"
~~~

The cpp currently uses a defensive include:

~~~cpp
#if __has_include("UI/Xaml/Control/Card/TitleCard.g.cpp")
#include "UI/Xaml/Control/Card/TitleCard.g.cpp"
#endif
~~~

Do not guess generated paths. C++/WinRT generation follows namespace and project-generation rules. If a g.h include fails, inspect the real Generated Files path before changing includes or project metadata.

## 11. Common failures: diagnose by generation stage

### 11.1 XamlTypeInfo.g.cpp says implementation is not a type

The root cause is often earlier than XamlTypeInfo itself:

- a normal C++ file was incorrectly associated with XAML code-behind;
- a same-named .xaml and runtimeclass collided in the generation pipeline;
- a required implementation declaration was not part of the expected inputs;
- stale Generated Files still reflect an older project structure.

Debugging order:

1. Identify the exact failing type in XamlTypeInfo.g.cpp.
2. Decide whether that type should come from IDL or XAML code-behind.
3. Inspect the corresponding vcxproj item type.
4. Look for unexpected Xxx.xaml.g.h files in Generated Files.
5. Clean, then Rebuild.
6. Only then investigate XamlTypeInfo generation itself.

### 11.2 LNK2019 for a property getter/setter

If Title()/Title(value) is declared in h but missing from cpp, the linker reports an unresolved external.

That is a C++ linkage problem, not a ControlTemplate problem. Fix the missing implementation first.

### 11.3 The control exists but renders nothing

Check in this order:

1. Constructor sets DefaultStyleKey.
2. The ResourceDictionary is actually merged by App.xaml.
3. Style TargetType matches the runtimeclass.
4. An implicit/default Style exists, or BasedOn resolves correctly.
5. The Style sets a ControlTemplate.
6. No resource-loading exception was swallowed during startup.

## 12. Minimum checklist for a new templated Control

- [ ] IDL runtimeclass derives from the intended WinUI base type.
- [ ] h includes the matching .g.h.
- [ ] cpp follows the repository's generated-file include pattern.
- [ ] Properties used by Style/Template/Binding are dependency properties.
- [ ] Each DependencyProperty is registered once.
- [ ] Constructor sets DefaultStyleKey.
- [ ] ResourceDictionary is loaded by App.xaml or another explicit resource entry point.
- [ ] No same-named Xxx.xaml exists unless a XAML code-behind type is intentional.
- [ ] VisualState logic does not accidentally overwrite caller-owned public DP values.
- [ ] Generated Files are inspected after Clean/Rebuild.
- [ ] The full application is started and the control is actually instantiated.

For deeper details about the generated XAML/C++ chain, see:

- docs/cppwinrt-xamltypeinfo-build-guide.md
