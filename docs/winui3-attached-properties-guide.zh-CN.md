# WinUI 3（C++/WinRT）附加属性实践指南

[English](./winui3-attached-properties-guide.md)

本文记录 OpenNet 中 WinUI 3 + C++/WinRT 附加属性（attached property）的实际实现方式，以及遇到 XAML 启动期错误时如何定位问题。重点不是罗列 API，而是说明一条可重复执行的实现和排错路径。

## 1. 先抓住核心：附加属性真正需要保证什么

一个附加属性最终仍由 DependencyProperty 系统保存。XAML 侧能否稳定读取它，取决于以下几个条件：

1. IDL 暴露了对应的静态 DependencyProperty getter，以及 GetXxx / SetXxx 访问器。
2. C++ 侧对同一个属性只注册一次，并始终返回同一个 DependencyProperty 实例。
3. RegisterAttached 的属性名、属性类型和 owner type 与公开元数据一致。
4. XAML 模板使用的绑定表达式能正确解析到 templated parent 上的附加属性。

这里最容易混淆的一点是“注册位置”和“注册次数”。

**真正的约束是只注册一次并保持属性标识稳定，而不是必须在某个特定时机提前注册。**

OpenNet 当前 NavItemIconHelper 使用类静态字段保存注册结果：

~~~cpp
static winrt::Microsoft::UI::Xaml::DependencyProperty s_selectedIconProperty;
~~~

并在 cpp 中注册一次：

~~~cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_selectedIconProperty =
    Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
        L"SelectedIcon",
        winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),
        winrt::xaml_typename<class_type>(),
        Microsoft::UI::Xaml::PropertyMetadata(nullptr));
~~~

getter 只返回已注册的对象：

~~~cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::SelectedIconProperty()
{
    return s_selectedIconProperty;
}
~~~

**不要这样写：**

~~~cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::SelectedIconProperty()
{
    return Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(...);
}
~~~

因为这样每次调用 getter 都会尝试重新注册同一个属性。

补充：函数内 static 也可以实现“一次注册、稳定返回”。OpenNet 的 TitleCard 普通依赖属性就采用这种模式。因此，不应把“在 getter 中出现 Register/RegisterAttached”本身视为错误；应检查它是否由 static 保存结果。

## 2. OpenNet 中一个完整附加属性需要改哪些文件

以 NavItemIconHelper.SelectedIcon 为例，当前实现涉及：

- OpenNet/Helpers/NavItemIconHelper.idl
- OpenNet/Helpers/NavItemIconHelper.h
- OpenNet/Helpers/NavItemIconHelper.cpp
- 使用点：OpenNet/Styles/NavigationView.xaml

### 2.1 IDL：先让 WinRT 元数据知道它存在

~~~idl
static Microsoft.UI.Xaml.DependencyProperty SelectedIconProperty{ get; };

static Object GetSelectedIcon(Microsoft.UI.Xaml.DependencyObject obj);
static void SetSelectedIcon(Microsoft.UI.Xaml.DependencyObject obj, Object value);
~~~

为什么需要这一步：XAML 类型系统看到的是 WinRT 元数据，而不是你的 C++ 实现头文件。只在 C++ 中写静态函数，不会自动让 XAML 解析器知道存在这个附加属性。

### 2.2 头文件：声明 DP getter 和 Get/Set

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

### 2.3 cpp：注册一次，再通过 GetValue/SetValue 访问

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

如果属性类型是 bool、Visibility 等值类型，需要 box_value / unbox_value，因为 DependencyObject::GetValue 和 SetValue 通过 IInspectable 传递值。

## 3. 在 ControlTemplate 中怎么读取附加属性

这里不要再使用“一刀切”的规则。

OpenNet 当前 NavigationView 模板中存在这种写法：

~~~xaml
helpers:NavItemIconHelper.SelectedIcon=
    "{TemplateBinding helpers:NavItemIconHelper.SelectedIcon}"
~~~

仓库当前实现能使用它，因此文档不应声称 TemplateBinding 对附加属性必然不可用。

但如果 XAML 编译器或运行时在某个模板位置无法正确解析 attached property，可以切换为显式 Binding 作为诊断和兼容手段：

~~~xaml
Content="{Binding (helpers:NavItemIconHelper.SelectedIcon),
          RelativeSource={RelativeSource Mode=TemplatedParent}}"
~~~

两者的关键差别：

- TemplateBinding 是模板专用的轻量绑定语法，适合直接把 templated parent 的属性传入模板。
- Binding + RelativeSource=TemplatedParent 使用完整 Binding 属性路径语义；括号语法明确告诉解析器这是 attached property。

因此实践顺序是：

1. 普通模板透传先保持现有 TemplateBinding 风格。
2. 如果错误明确指向 attached property 解析，再用显式 Binding 做最小替换验证。
3. 不要在没有复现证据时全局替换所有 TemplateBinding。

## 4. 启动期 XAML 错误怎么定位

这类错误常见表现包括：

- 0x802B000A
- 0x80004005
- 0xC000027B / fail-fast
- The property 'Xxx' was not found in type 'Yyy'
- 找不到某个 Style 或 Resource key

不要从整个应用代码开始猜。按资源加载链缩小范围。

### 第一步：判断是不是资源字典加载失败

临时减少 App.xaml 中的 MergedDictionaries，确认错误是否随某个字典消失。

为什么有效：应用启动时大量样式和模板会一次性解析。只要其中一个模板存在无法解析的类型或属性，异常表面上可能出现在完全不相关的启动路径。

注意：移除资源字典可能产生新的“缺少资源”错误。此时要区分：

- 原始错误：属性/类型解析失败。
- 排查引入的错误：依赖的 Style/Brush 被一起移除了。

### 第二步：进入字典内部做最小化

如果已经锁定 NavigationView.xaml：

1. 保留目标 Style key。
2. 把 ControlTemplate 临时替换为最小 Grid。
3. 确认应用可启动。
4. 分段恢复模板。
5. 直到恢复某一段后错误再次出现。

这比仅依赖 fail-fast 栈有效，因为栈通常只能说明错误发生在 XAML 解析/实例化阶段，不能直接告诉你哪条 markup expression 有问题。

### 第三步：如果错误指向附加属性

按以下顺序检查：

1. IDL 是否有 XxxProperty、GetXxx、SetXxx。
2. 生成的元数据名称是否与 XAML 使用名称一致。
3. RegisterAttached 是否只执行一次。
4. owner type 是否为正确 runtimeclass。
5. 属性类型是否一致。
6. XAML 模板表达式是否能解析 attached property。
7. Clean/Rebuild 后再验证，排除旧 Generated Files 干扰。

## 5. 修改附加属性时的最小验证流程

新增或修改一个附加属性后，至少做以下验证：

1. **编译验证**：IDL、C++/WinRT 生成代码和 XAML 编译全部通过。
2. **直接设置验证**：在一个 NavigationViewItem 或普通 DependencyObject 上直接设置附加属性。
3. **模板透传验证**：确认 ControlTemplate 内能读取到同一个值。
4. **默认值验证**：不设置属性时，PropertyMetadata 默认值符合预期。
5. **值类型验证**：bool / enum 使用正确的 box/unbox。
6. **启动验证**：执行一次完整应用启动，而不是只确认单个 C++ TU 编译通过。

## 6. 本仓库参考实现

当前最直接的参考：

- OpenNet/Helpers/NavItemIconHelper.idl
- OpenNet/Helpers/NavItemIconHelper.h
- OpenNet/Helpers/NavItemIconHelper.cpp
- OpenNet/Styles/NavigationView.xaml

如果问题是 XamlTypeInfo 或生成文件而不是 attached property 本身，请继续阅读：

- docs/cppwinrt-xamltypeinfo-build-guide.md

## 7. 提交前检查

- [ ] 同一 DependencyProperty 只注册一次。
- [ ] getter 每次返回同一个 DependencyProperty。
- [ ] IDL、C++ 名称和 XAML 名称完全一致。
- [ ] owner type 和属性类型正确。
- [ ] 值类型正确 box/unbox。
- [ ] 模板绑定在实际目标模板中验证过。
- [ ] Clean/Rebuild 后无意外生成文件或旧生成物干扰。
- [ ] 完整启动至少验证一次。
