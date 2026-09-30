# WinUI 3（C++/WinRT）自定义 Control 实践指南

[English](./xaml-custom-controls-guide.md)

本文以 OpenNet 的 TitleCard 为主例，说明如何实现“C++/WinRT runtimeclass + DependencyProperty + XAML ControlTemplate”的可复用控件。重点是当前仓库实际使用的文件结构、生成链和排错步骤。

## 1. 先判断：你要的是 templated Control 还是带 code-behind 的 XAML 控件

TitleCard 属于 **templated Control**：

- 类型由 IDL + C++ 实现。
- 外观由 ResourceDictionary 中的 Style / ControlTemplate 定义。
- 不需要 TitleCard.xaml、TitleCard.xaml.h、TitleCard.xaml.cpp。

当前文件：

- OpenNet/UI/Xaml/Control/Card/TitleCard.idl
- OpenNet/UI/Xaml/Control/Card/TitleCard.h
- OpenNet/UI/Xaml/Control/Card/TitleCard.cpp
- OpenNet/UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml

为什么要先分清这一点：如果给一个纯 templated Control 额外创建同名 TitleCard.xaml，就会引入另一条 XAML code-behind 生成链，增加 Generated Files、XamlTypeInfo 和类型注册之间的冲突风险。

如果你的组件本质上需要自己的 XAML tree 和 code-behind 生命周期，那应当使用 UserControl 或其他 XAML-backed 类型；不要为了复用样式而混用两套模型。

## 2. 一次完整实现的推荐顺序

实现新控件时，按下面顺序更容易定位错误：

1. 定义 IDL runtimeclass。
2. 让项目正确包含 idl / h / cpp。
3. 先实现构造函数和最少的 DependencyProperty。
4. 确认 C++/WinRT 生成代码可编译。
5. 再加入 ResourceDictionary 和 ControlTemplate。
6. 最后接入 App.xaml 或其他资源入口。
7. 完整启动应用验证模板加载。

这样做的原因是：IDL/生成代码错误和 XAML 模板错误属于不同阶段。一次把所有内容都加上，出现 XamlTypeInfo.g.cpp 错误时很难判断到底是哪一层先坏了。

## 3. IDL：只定义公开 WinRT 表面

TitleCard 当前 IDL：

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

IDL 的作用是产生 WinRT 元数据和 C++/WinRT 生成声明。XAML 能识别这个自定义类型，依赖的就是这层元数据。

实践建议：

- 只把真正需要从 XAML/WinRT 访问的成员放进 IDL。
- 内部 helper、缓存和模板部件不要为了方便全部暴露成 runtimeclass API。
- 修改 IDL 后优先检查 Generated Files 是否符合预期，再继续排查 C++ 实现。

## 4. DependencyProperty：模板要消费的可样式化属性应走 DP

TitleCard 的 Title、TitleContent、Content、DividerVisibility 都通过 DependencyProperty 保存。

以 Title 为例：

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

这里 function-local static 很重要：Register 只执行一次，之后始终返回同一个 DP。

CLR 风格的普通 C++ 成员变量不能替代 DP，因为 ControlTemplate、Style、Binding、动画和属性优先级系统都建立在 DependencyProperty 上。

### 4.1 getter / setter 只做属性系统访问

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

不要在简单 getter/setter 中重复实现一套独立状态，否则 C++ 成员值和 XAML 属性系统值可能分叉。

## 5. DefaultStyleKey：告诉 WinUI 去找哪个默认 Style

构造函数当前这样做：

~~~cpp
TitleCard::TitleCard()
{
    DefaultStyleKey(winrt::box_value(winrt::xaml_typename<class_type>()));
}
~~~

没有 DefaultStyleKey 时，即使资源字典里存在 TargetType=TitleCard 的默认 Style，控件也可能无法按预期取得默认模板。

## 6. ResourceDictionary：当前 OpenNet 的真实接线方式

TitleCard 模板位于：

- OpenNet/UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml

并由 **OpenNet/App.xaml** 合并：

~~~xaml
<ResourceDictionary
    Source="ms-appx:///UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml" />
~~~

旧指南把 Generic.xaml 写成当前接入点是不准确的。Generic.xaml 是自定义控件库常见的默认样式入口，但 **OpenNet 当前 TitleCard 实际走 App.xaml**。

因此新增控件时不要只看“理论上推荐 Generic.xaml”，而应先保持项目现有资源组织方式一致。若以后要迁移到 Generic.xaml，应该作为单独的资源架构改动处理。

## 7. TemplateBinding：什么时候适合用

TitleCard 模板中的典型写法：

~~~xaml
<TextBlock Text="{TemplateBinding Title}" />

<ContentPresenter
    Content="{TemplateBinding TitleContent}" />

<ContentPresenter
    Content="{TemplateBinding Content}" />
~~~

TemplateBinding 适合把 templated parent 上的 DP 直接传给模板元素。

如果你需要 converter、复杂 property path、FallbackValue 等完整 Binding 能力，再使用 Binding + RelativeSource=TemplatedParent。

不要因为某个属性“能在 C++ getter 中返回值”就认为 TemplateBinding 一定能看到它；模板系统需要对应的依赖属性元数据。

## 8. VisualState：内部视觉状态不要意外覆盖调用者输入

TitleCard 当前 OnApplyTemplate 后调用 SetVisualStates：

~~~cpp
void TitleCard::OnApplyTemplate()
{
    TitleCardT<TitleCard>::OnApplyTemplate();
    SetVisualStates();
}
~~~

模板中定义：

~~~xaml
<VisualState x:Name="TitleGridVisible" />
<VisualState x:Name="TitleGridCollapsed">
    <VisualState.Setters>
        <Setter Target="TitleGrid.Visibility" Value="Collapsed" />
    </VisualState.Setters>
</VisualState>
~~~

这是推荐方向：**让 VisualState 修改模板部件，而不是把内部派生状态写回公共 DP。**

当前 TitleCard::SetVisualStates 还会直接写 DividerVisibility。这样会覆盖调用方对 DividerVisibility 的显式设置，因此后续重构时应优先考虑：

- 调用方输入：保留在公共 DP。
- 内部派生视觉状态：通过 VisualState setter 修改模板元素。
- 如果必须区分两者，新增只读/内部状态，而不是复用同一个可写公共 DP。

这条原则能避免“调用者明明设置了属性，但控件内部又悄悄改回去”的问题。

## 9. 项目文件：不要把普通 h/cpp 伪装成 XAML code-behind

OpenNet.vcxproj 当前 TitleCard.cpp 的依赖关系指向 TitleCard.idl，这属于项目组织关系：

~~~xml
<ClCompile Include="UI\Xaml\Control\Card\TitleCard.cpp">
  <DependentUpon>UI\Xaml\Control\Card\TitleCard.idl</DependentUpon>
</ClCompile>
~~~

真正要避免的是把普通 TitleCard.h / TitleCard.cpp 配成某个不存在的 TitleCard.xaml 的 code-behind，从而让 XAML 编译器错误地生成 TitleCard.xaml.g.h / .g.cpp。

如果出现 Generated Files 中意外多出同名 xaml 生成物，先检查 vcxproj 的 Page、ApplicationDefinition、DependentUpon 和文件命名关系。

## 10. g.h / g.cpp：只包含与你的 runtimeclass 对应的生成文件

头文件：

~~~cpp
#include "UI/Xaml/Control/Card/TitleCard.g.h"
~~~

cpp 中仓库当前使用防御式包含：

~~~cpp
#if __has_include("UI/Xaml/Control/Card/TitleCard.g.cpp")
#include "UI/Xaml/Control/Card/TitleCard.g.cpp"
#endif
~~~

不要猜生成路径。C++/WinRT 生成路径由 namespace / 项目生成规则决定。遇到找不到 g.h 时，应先查看 Generated Files 中真实路径，再回头修 include 或项目元数据。

## 11. 常见故障：按生成阶段定位，而不是看见 XamlTypeInfo 就怪 XamlTypeInfo

### 11.1 XamlTypeInfo.g.cpp 报 implementation 不是类型

常见原因不是 XamlTypeInfo 自己写坏了，而是更早的类型发现或生成输入出现了问题，例如：

- 项目项把普通 C++ 文件错误关联成 XAML code-behind。
- 同名 .xaml 与 runtimeclass 造成生成链冲突。
- 需要的实现头没有进入生成器预期输入。
- Generated Files 中仍残留旧结构。

排查顺序：

1. 找 XamlTypeInfo.g.cpp 中失败的具体类型。
2. 确认这个类型应当来自 IDL 还是 XAML code-behind。
3. 检查对应 vcxproj 项类型。
4. 检查 Generated Files 是否出现意外的 Xxx.xaml.g.h。
5. Clean，再 Rebuild。
6. 仍失败再分析 XamlTypeInfo 生成规则。

### 11.2 LNK2019：属性 getter/setter 未解析

如果 h 中声明了 Title()/Title(value)，但 cpp 没有实现，链接器会报未解析外部符号。

这属于 C++ 链接问题，不是 XAML 模板问题。先补齐实现，再看 XAML。

### 11.3 控件存在但完全没有视觉

按顺序检查：

1. 构造函数是否设置 DefaultStyleKey。
2. ResourceDictionary 是否真的被 App.xaml 合并。
3. Style TargetType 是否匹配 runtimeclass。
4. 默认 Style 是否存在（无 x:Key 的 Style 或正确的 BasedOn 关系）。
5. ControlTemplate 是否被设置。
6. 启动时是否有被吞掉的资源加载异常。

## 12. 新增一个 templated Control 的最小实践清单

- [ ] IDL runtimeclass 继承正确的 WinUI 基类。
- [ ] h 包含对应 .g.h。
- [ ] cpp 的生成文件包含方式与仓库其他 runtimeclass 一致。
- [ ] 需要样式/模板/绑定参与的属性使用 DependencyProperty。
- [ ] DependencyProperty 每个只注册一次。
- [ ] 构造函数设置 DefaultStyleKey。
- [ ] ResourceDictionary 被 App.xaml 或明确的资源入口加载。
- [ ] 没有额外创建同名 Xxx.xaml，除非你确实要 XAML code-behind 类型。
- [ ] VisualState 修改模板部件，不无意覆盖调用者的公共 DP。
- [ ] Clean/Rebuild 后检查 Generated Files。
- [ ] 完整启动并实际实例化该控件。

更多生成链细节见：

- docs/cppwinrt-xamltypeinfo-build-guide.md
