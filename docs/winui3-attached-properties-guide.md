# WinUI 3 Attached Properties in C++/WinRT: Practical Guide

[简体中文](./winui3-attached-properties-guide.zh-CN.md)

This document describes how OpenNet implements WinUI 3 attached properties with C++/WinRT and how to diagnose XAML startup failures related to them. The goal is a repeatable implementation and debugging workflow rather than an API catalog.

## 1. Core invariant: what an attached property actually needs

An attached property is still stored by the DependencyProperty system. For XAML to use it reliably, the following must remain consistent:

1. IDL exposes the static DependencyProperty getter and the GetXxx / SetXxx accessors.
2. C++ registers the property once and keeps returning the same DependencyProperty instance.
3. The name, property type, and owner type passed to RegisterAttached match the public metadata.
4. The template expression can resolve the attached property on the templated parent.

The important distinction is **registration count**, not a particular registration location.

OpenNet currently stores NavItemIconHelper properties in static class fields:

~~~cpp
static winrt::Microsoft::UI::Xaml::DependencyProperty s_selectedIconProperty;
~~~

The property is registered once in the cpp file:

~~~cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_selectedIconProperty =
    Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
        L"SelectedIcon",
        winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),
        winrt::xaml_typename<class_type>(),
        Microsoft::UI::Xaml::PropertyMetadata(nullptr));
~~~

The getter only returns that object:

~~~cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::SelectedIconProperty()
{
    return s_selectedIconProperty;
}
~~~

Do not register a new property on every getter call:

~~~cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::SelectedIconProperty()
{
    return Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(...);
}
~~~

A function-local static is also a valid way to get one-time registration and stable identity. OpenNet's TitleCard uses that pattern for normal dependency properties. Therefore, seeing Register or RegisterAttached inside a getter is not automatically wrong; the question is whether the result is cached in a static object.

## 2. Files involved in one OpenNet attached property

For NavItemIconHelper.SelectedIcon, the implementation spans:

- OpenNet/Helpers/NavItemIconHelper.idl
- OpenNet/Helpers/NavItemIconHelper.h
- OpenNet/Helpers/NavItemIconHelper.cpp
- Usage: OpenNet/Styles/NavigationView.xaml

### 2.1 IDL: expose the property to the WinRT/XAML type system

~~~idl
static Microsoft.UI.Xaml.DependencyProperty SelectedIconProperty{ get; };

static Object GetSelectedIcon(Microsoft.UI.Xaml.DependencyObject obj);
static void SetSelectedIcon(Microsoft.UI.Xaml.DependencyObject obj, Object value);
~~~

Why this matters: XAML consumes WinRT metadata, not your C++ header directly. C++ static methods alone do not make an attached property visible to the XAML type system.

### 2.2 Header: declare the DP getter and accessors

~~~cpp
static winrt::Microsoft::UI::Xaml::DependencyProperty SelectedIconProperty();

static winrt::Windows::Foundation::IInspectable GetSelectedIcon(
    winrt::Microsoft::UI::Xaml::DependencyObject const& obj);

static void SetSelectedIcon(
    winrt::Microsoft::UI::Xaml::DependencyObject const& obj,
    winrt::Windows::Foundation::IInspectable const& value);

private:
    static winrt::Microsoft::UI::Xaml::DependencyProperty s_selectedIconProperty;
~~~

### 2.3 C++: register once, then use GetValue/SetValue

~~~cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_selectedIconProperty =
    Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
        L"SelectedIcon",
        winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),
        winrt::xaml_typename<class_type>(),
        Microsoft::UI::Xaml::PropertyMetadata(nullptr));

IInspectable NavItemIconHelper::GetSelectedIcon(
    Microsoft::UI::Xaml::DependencyObject const& obj)
{
    return obj.GetValue(SelectedIconProperty());
}

void NavItemIconHelper::SetSelectedIcon(
    Microsoft::UI::Xaml::DependencyObject const& obj,
    IInspectable const& value)
{
    obj.SetValue(SelectedIconProperty(), value);
}
~~~

Value types such as bool and Visibility need box_value / unbox_value because DependencyObject::GetValue and SetValue exchange values through IInspectable.

## 3. Reading attached properties inside a ControlTemplate

Do not treat this as a universal “TemplateBinding is forbidden” rule.

OpenNet currently contains this form in NavigationView.xaml:

~~~xaml
helpers:NavItemIconHelper.SelectedIcon=
    "{TemplateBinding helpers:NavItemIconHelper.SelectedIcon}"
~~~

That is part of the current working template, so the repository documentation should not claim that TemplateBinding can never work with attached properties.

If the XAML compiler or runtime fails to resolve an attached property at a particular template location, use an explicit Binding as a diagnostic or compatibility fallback:

~~~xaml
Content="{Binding (helpers:NavItemIconHelper.SelectedIcon),
          RelativeSource={RelativeSource Mode=TemplatedParent}}"
~~~

The distinction is useful:

- TemplateBinding is the lightweight template-specific syntax for forwarding a property from the templated parent.
- Binding + RelativeSource=TemplatedParent uses the full Binding property-path grammar; parentheses explicitly identify an attached property.

Practical order of operations:

1. Keep the existing TemplateBinding form for ordinary property forwarding.
2. If the failure specifically points to attached-property resolution, replace only the suspect expression with explicit Binding.
3. Do not globally rewrite every TemplateBinding without a reproducer.

## 4. Diagnosing startup-time XAML failures

Typical symptoms include:

- 0x802B000A
- 0x80004005
- 0xC000027B / fail-fast
- The property 'Xxx' was not found in type 'Yyy'
- Missing Style or resource key errors

Do not start by guessing across the whole application. Narrow the resource-loading path.

### Step 1: isolate the resource dictionary

Temporarily reduce App.xaml MergedDictionaries and check whether removing one dictionary removes the original failure.

Why this works: startup can instantiate or parse a large number of styles and templates. A bad type or property reference can surface as a generic startup failure.

Be careful not to confuse the original failure with a new missing-resource error introduced by removing a dictionary.

### Step 2: minimize the failing dictionary

After identifying a dictionary such as NavigationView.xaml:

1. Keep the target Style key.
2. Replace the ControlTemplate with a minimal Grid.
3. Verify that startup succeeds.
4. Restore the template in sections.
5. Stop when one section reintroduces the failure.

This is usually more informative than a fail-fast stack, which often identifies the XAML activation path but not the exact markup expression.

### Step 3: if the error points to an attached property

Check, in this order:

1. IDL contains XxxProperty, GetXxx, and SetXxx.
2. Generated metadata names match the XAML name.
3. RegisterAttached executes only once.
4. The owner type is the intended runtimeclass.
5. The registered property type matches the accessor and metadata.
6. The template expression resolves the attached property.
7. Clean/Rebuild removes stale Generated Files from the equation.

## 5. Minimum verification after changing an attached property

At minimum, verify:

1. **Build**: IDL, C++/WinRT generation, and XAML compilation all succeed.
2. **Direct set/get**: the attached property works on a NavigationViewItem or another DependencyObject.
3. **Template forwarding**: the ControlTemplate sees the same value.
4. **Default value**: PropertyMetadata behaves correctly when the property is not set.
5. **Value types**: bool / enum values use the correct boxing.
6. **Full startup**: run the application once; a successful C++ translation unit build is not enough.

## 6. Repository references

Use these files as the current implementation reference:

- OpenNet/Helpers/NavItemIconHelper.idl
- OpenNet/Helpers/NavItemIconHelper.h
- OpenNet/Helpers/NavItemIconHelper.cpp
- OpenNet/Styles/NavigationView.xaml

For XamlTypeInfo and generated-file failures, also read:

- docs/cppwinrt-xamltypeinfo-build-guide.md

## 7. Pre-commit checklist

- [ ] Each DependencyProperty is registered exactly once.
- [ ] The getter always returns the same DependencyProperty.
- [ ] IDL, C++, and XAML names match exactly.
- [ ] Owner type and property type are correct.
- [ ] Value types are boxed/unboxed correctly.
- [ ] The binding form was verified in the actual target template.
- [ ] Clean/Rebuild does not reveal stale or unexpected generated files.
- [ ] Full application startup was tested at least once.
