# OpenNet WinUI 3 自定义控件与样式编写指南（C++/WinRT）

> 适用范围：本仓库 `OpenNet`（WinUI 3 + C++/WinRT）。
>
> 目标：用“自定义 `Control` + `Generic.xaml`/资源字典模板”的方式实现可复用 UI 组件（类似 `TitleCard`）。
>
> 关键原则：
> - 控件本体（C++/WinRT）：只负责 `DependencyProperty`、默认样式键、模板应用时的逻辑（`OnApplyTemplate`）等。
> - 视觉模板（XAML）：放在 `Themes/Generic.xaml` 或拆分到 `*_ResourceDictionary.xaml` 后由 `Generic.xaml` / `App.xaml` 合并。
> - **不要**把控件当作 `Control.xaml`（没有 `*.xaml` + `*.xaml.h/.xaml.cpp` 代码后台）来构建；避免触发不必要的 XAML 代码生成链。

---

## 0. 你需要了解的文件角色

- `*.idl`
  - 定义 WinRT `runtimeclass`（属性、构造函数、继承关系）。
  - 会生成 `Generated Files\...\*.g.h/*.g.cpp`。

- `*.h/*.cpp`
  - 你的控件实现代码。
  - `*.h` 必须 `#include "...*.g.h"`。
  - `*.cpp` 一般包含 `*.g.cpp`（用 `__has_include` 防御）。

- `Themes/Generic.xaml`
  - WinUI 控件默认样式入口。
  - `DefaultStyleKey` 指向的样式通常都在这里（或其合并的字典）。

- `*_ResourceDictionary.xaml`
  - 推荐用于拆分控件样式/模板。
  - `Generic.xaml` 或 `App.xaml` 中 `MergedDictionaries` 引用。

---

## 1. 创建一个自定义控件：目录与命名

以 `TitleCard` 为例（命名空间：`OpenNet.UI.Xaml.Control.Card`）：

推荐目录结构：

- `OpenNet/UI/Xaml/Control/Card/TitleCard.idl`
- `OpenNet/UI/Xaml/Control/Card/TitleCard.h`
- `OpenNet/UI/Xaml/Control/Card/TitleCard.cpp`
- `OpenNet/UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml`
- `OpenNet/Themes/Generic.xaml`（合并字典引用）

命名约定：

- 控件类名：`TitleCard`
- 样式字典：`TitleCard_ResourceDictionary.xaml`
- 样式 Key（可选）：`DefaultTitleCardStyle`（或 `TitleCardStyle`）

---

## 2. 定义 IDL：runtimeclass 与属性

在 `TitleCard.idl` 中定义（示例）：

- `runtimeclass TitleCard : Microsoft.UI.Xaml.Controls.Control`
- `TitleCard();`
- 属性：`String Title; Object TitleContent; Object Content; Microsoft.UI.Xaml.Visibility DividerVisibility;`

注意：

- `String`/`Object`/`Visibility` 都会映射到 WinRT 侧类型。
- `Content` 这种名字会与 `Control::Content` 的常心理解冲突，但在 WinUI `Control` 上并不是内置属性；可以保留。

---

## 3. 生成代码与项目引用

### 3.1 `.vcxproj` 包含关系

必须保证 `TitleCard.idl/.h/.cpp` 被项目编译：

- `TitleCard.cpp` 在 `<ClCompile Include=...>`
- `TitleCard.h` 在 `<ClInclude Include=...>`
- `TitleCard.idl` 在 `<Midl Include=...>`（或仓库已有自动规则）

### 3.2 避免错误的 `DependentUpon`

**不要**把 `TitleCard.h` 设置为依赖 `TitleCard.cpp`：

- 这类关系容易触发 XAML 编译/生成链误判，生成空的 `Generated Files/.../TitleCard.xaml`，并导致 `XamlTypeInfo.g.cpp` 引用不完整。

推荐：

- `*.xaml.h` 才应该 `DependentUpon` `*.xaml`
- 普通 `*.h` 不需要 `DependentUpon`（或仅用于 VS UI 分组，不要和 `*.xaml` 形成链）

---

## 4. C++/WinRT 控件实现：DependencyProperty + 默认样式键

### 4.1 DefaultStyleKey

在构造函数中设置：

- `DefaultStyleKey(winrt::box_value(winrt::xaml_typename<class_type>()));`

这是 WinUI 查找默认样式的关键。

### 4.2 属性必须是 DependencyProperty（用于 TemplateBinding）

如果你的模板里使用：

- `{TemplateBinding Title}`
- `{TemplateBinding TitleContent}`
- `{TemplateBinding DividerVisibility}`

那么这些属性必须是 `DependencyProperty`，否则 `TemplateBinding` 不能稳定工作。

推荐实现模式：

- `static DependencyProperty TitleProperty()`
- `hstring Title(); void Title(hstring const&)`
- `static void OnVisualPropertyChanged(DependencyObject const&, DependencyPropertyChangedEventArgs const&)`

### 4.3 OnApplyTemplate 与 VisualState

若你需要像 C# 示例那样应用 VisualState：

- 实现 `OnApplyTemplate()`
- 调用 `VisualStateManager::GoToState(*this, L"StateName", true)`

注：在 C++/WinRT 里 `OnApplyTemplate()` 属于 `IControlOverrides`/`ControlT` 体系，照着生成的 `*.g.h` 中基类即可 override。

---

## 5. 样式与模板：资源字典写法

### 5.1 推荐写法：`*_ResourceDictionary.xaml`

示例结构：

- `xmlns:local="using:OpenNet.UI.Xaml.Control.Card"`
- `Style x:Key="DefaultTitleCardStyle" TargetType="local:TitleCard"`
- 提供一个默认 Style：
  - `<Style BasedOn="{StaticResource DefaultTitleCardStyle}" TargetType="local:TitleCard" />`

### 5.2 ControlTemplate 内的 TemplateBinding

原则：

- 控件 DP：`Title`/`TitleContent`/`DividerVisibility`/`Content`
- 模板使用 `{TemplateBinding ...}`

常见陷阱：

- 若属性不是 DP，`TemplateBinding` 可能完全不生效或行为异常。

### 5.3 VisualStateGroup 命名与状态

建议：

- `VisualStateGroup x:Name="TitleGridVisibilityStates"`
- `VisualState x:Name="TitleGridVisible"`
- `VisualState x:Name="TitleGridCollapsed"`

状态里用 `Setter Target="TitleGrid.Visibility" Value="Collapsed"`。

控件代码在需要时切换到对应 state。

---

## 6. 将控件模板接入应用资源

两种选择：

### 6.1 使用 `Themes/Generic.xaml`（推荐）

在 `Generic.xaml` 中合并：

```xml
<ResourceDictionary.MergedDictionaries>
  <ResourceDictionary Source="ms-appx:///UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml" />
</ResourceDictionary.MergedDictionaries>
```

优点：

- 遵循 WinUI 控件默认样式加载机制。

### 6.2 在 `App.xaml` 合并（适合应用级样式）

你当前项目也可在 `App.xaml`：

```xml
<ResourceDictionary Source="ms-appx:///UI/Xaml/Control/Card/TitleCard_ResourceDictionary.xaml" />
```

注意：

- 如果你希望控件在任意位置都能自动拿到默认样式，更建议走 `Generic.xaml`。

---

## 7. 使用控件（XAML）

引入命名空间：

```xml
xmlns:card="using:OpenNet.UI.Xaml.Control.Card"
```

使用示例：

```xml
<card:TitleCard Title="Downloads" DividerVisibility="Visible">
  <card:TitleCard.TitleContent>
    <Button Content="More" />
  </card:TitleCard.TitleContent>

  <card:TitleCard.Content>
    <TextBlock Text="Hello" />
  </card:TitleCard.Content>
</card:TitleCard>
```

如果支持 `ContentProperty`（内容属性），可以将 `ContentProperty` 标记为内容属性：

- C#: `[ContentProperty(Name=nameof(Content))]`
- C++/WinRT：通常通过 IDL/元数据与 XAML 解析器支持（若需要，建议后续单独做一条规范）。

---

## 8. 常见问题排查

### 8.1 `XamlTypeInfo.g.cpp` 出现 `ActivateLocalType<...implementation::TitleCard>` 编译错误

表现：

- `C3083 'implementation': ... must be a type`

原因：

- `XamlTypeInfo.g.cpp` 注册了本地类型，但未包含控件实现声明头。
- 常由错误的项目项配置/依赖关系触发（例如 `TitleCard.h DependentUpon TitleCard.cpp`），导致生成器误判并生成空 `TitleCard.xaml`。

修复建议：

- 检查 `.vcxproj`，移除不合理的 `DependentUpon`。
- Clean + Rebuild，确认不再生成 `Generated Files/.../TitleCard.xaml`。

### 8.2 链接错误 `LNK2019`：TitleCard 的 get/put 未解析

原因：

- 你在 `TitleCard.h` 声明了属性方法，但 `.cpp` 没实现。

修复建议：

- 补齐实现，或改为 DP 并实现 `GetValue/SetValue`。

---

## 9. 提交前检查清单

- [ ] `*.idl` 定义完整，构建能生成 `*.g.h/*.g.cpp`。
- [ ] 控件属性若用于 `TemplateBinding`，必须是 DP。
- [ ] `DefaultStyleKey` 设置正确。
- [ ] `Generic.xaml` 或 `App.xaml` 合并了控件资源字典。
- [ ] `.vcxproj` 中没有不合理的 `DependentUpon` 导致生成空 `*.xaml`。
- [ ] Clean + Rebuild 后，`Generated Files` 中没有意外生成的 `TitleCard.xaml`（除非你确实创建了这个控件的 XAML 文件）。

---

## 附：是否将本文件作为仓库规范

如需将本指南作为仓库默认协作规范，可在 `.github/` 下保留该文件，并在 `README.md` 中链接。
