# C++/WinRT + WinUI 3（XAML）生成代码/链接问题排查与开发指南

> 适用场景：WinUI 3 Desktop（C++/WinRT）项目中新增 `runtimeclass`、附加属性（attached property）、自定义控件/页面等。
>
> 目标：解释为什么会出现 `LNK2019 unresolved external symbol`、`XamlTypeInfo.g.cpp` 相关问题；以及如何用“正确的方式”新增代码和维护 `vcxproj` 里的生成文件编译项，避免“生成了 `.g.cpp` 但未编译进项目导致链接失败”。

> 备注：你在 IDE 里可能会看到诸如 `.../MSVC/.../crt/src/vcruntime/throw.cpp` 的文件。
> 这类文件属于 **MSVC/CRT 的实现源码**（编译器工具链/运行库），与本仓库业务代码无关；排查/修复本项目问题时不应修改、也不应把它加入工程或提交到仓库。

---

## 1. 你看到的现象，背后的真实原因

### 1.1 典型报错长什么样
常见链接错误（示例）：

- `LNK2019 unresolved external symbol winrt::...::Implicit::GetAnimations(...)`
- `LNK2019 unresolved external symbol winrt::...::Implicit::SetAnimations(...)`

并且会提示这些符号被 `OpenNet\Generated Files\XamlTypeInfo.g.cpp` 中的函数引用。

### 1.2 为什么“我已经写了实现”，链接器还说找不到
在 C++/WinRT 中通常会同时存在两层类型：

- **投影(public)层**：`winrt::OpenNet::UI::Xaml::Media::Animations::Implicit`
- **implementation 层**：`winrt::OpenNet::UI::Xaml::Media::Animations::implementation::Implicit`

你手写 `Implicit.cpp` 时实现的一般是 implementation 层（`implementation::Implicit::GetAnimations`）。

但 `XamlTypeInfo.g.cpp` 会调用 **public 层** 的 `Implicit::GetAnimations/SetAnimations`。

public 层的这些函数通常不是你手写的，而是由生成文件（如 `Implicit.g.cpp`）提供**转发定义**：

- `winrt::OpenNet::...::Implicit::GetAnimations(...) { return ...implementation::Implicit::GetAnimations(...); }`

所以：

- 如果 `Implicit.g.cpp` **生成了但没有参与编译/链接** → 链接器找不到 public 层符号 → LNK2019。

这就是你后来“把 `.g.cpp` 加进项目后就能编译并运行”的原因。

---

## 2. WinUI 3 + C++/WinRT 项目里常见的“生成代码类型”

在 WinUI 3 C++/WinRT 项目中，一般会看到几类生成文件：

### 2.1 `*.xaml.g.h / *.xaml.g.cpp`
- 由 XAML 编译器生成
- 用于页面/控件的 `InitializeComponent`、绑定信息等

### 2.2 `XamlTypeInfo.g.cpp`（以及相关的 `*.xaml.g.h`）
- 由 XAML 编译/工具链生成
- 里面包含 **XAML 运行期元数据提供器** 的实现
- 用于让 XAML 运行时：
  - 根据字符串类型名创建对象（反射式创建）
  - 找到属性、依赖属性、附加属性的 getter/setter

### 2.3 C++/WinRT 的 `*.g.h / *.g.cpp`
- 由 C++/WinRT（或者与其集成的生成步骤）生成
- 这类文件非常关键：
  - **提供 public 投影层到 implementation 层的转发**
  - 提供激活工厂 `winrt_make_*` 等入口

你贴出的 `OpenNet\Generated Files\UI\Xaml\Media\Animations\Implicit.g.cpp` 就属于这一类。

---

## 3. 为什么“用项目模板添加”更安全

### 3.1 模板/向导通常会帮你做的事
在 VS 里用“添加新项（C++/WinRT Runtime Component / WinUI 控件/页面模板）”之类方式创建类型时，向导一般会：

1. 创建 `*.idl`
2. 创建 `*.h / *.cpp`（implementation）
3. 自动生成并把 `*.g.h / *.g.cpp` 产物纳入构建
4. 修改 `vcxproj`：
   - 把新增 `.cpp` 加入 `ClCompile`
   - 把部分生成输出目录加入 `AdditionalIncludeDirectories`
   - 确保 MIDL/CppWinRT 目标在正确的时机运行

如果你手工只写 `.idl` + `.h/.cpp`，但没有让生成的 `*.g.cpp` 被编译，就很容易遇到“编译成功、链接失败”。

### 3.2 为什么“纯 C++ 模块”一般不会进 XamlTypeInfo
`XamlTypeInfo.g.cpp` 的职责是：

- 给 XAML 引擎提供“类型/成员信息”。

因此：
- 只有 **被 XAML 用到** 的类型（页面、控件、MarkupExtension、附加属性提供者等）才会出现在 `XamlTypeInfo.g.cpp` 的 `TypeInfos/MemberInfos` 里。
- 纯 C++ 业务类（例如非 WinRT 类型、非 XAML 类型）通常不会出现在 `XamlTypeInfo`。

换句话说：
- `XamlTypeInfo` 不是“项目中所有类的清单”，它是“XAML 需要反射/动态访问的类清单”。

---

## 4. 附加属性（Attached Property）与链接错误的关系

## 4.0 附加属性的“完整写法框架”（IDL / C++ / XAML 对照）

这一节用你这次的 `Implicit.Animations` 做“模板化/可复制”的框架，便于以后新加附加属性时照着写。

### 4.0.1 IDL（对外契约）

典型写法（示意，保持和你项目一致的设计）：

- `runtimeclass` 作为附加属性的声明宿主（DeclaringType）
- `Value` 类型可以是你自己的 `runtimeclass`（例如集合容器），也可以是原生 WinRT 类型

你项目里类似：

- `static Microsoft.UI.Xaml.DependencyProperty AnimationsProperty{ get; };`
- `static ImplicitAnimationSet GetAnimations(DependencyObject obj);`
- `static void SetAnimations(DependencyObject obj, ImplicitAnimationSet value);`

### 4.0.2 头文件（`*.h`）里需要的静态签名

implementation 层通常写成（与你当前 `Implicit.h` 相同风格）：

- `static winrt::Microsoft::UI::Xaml::DependencyProperty AnimationsProperty();`
- `static winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet GetAnimations(winrt::Microsoft::UI::Xaml::DependencyObject const& obj);`
- `static void SetAnimations(winrt::Microsoft::UI::Xaml::DependencyObject const& obj, winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet const& value);`

要点：

- 这里写的是 **implementation 层**（`namespace ...::implementation`）的静态成员。
- XAML/投影层最终需要的是 **public 层** `winrt::...::Implicit::GetAnimations/SetAnimations`。
- public 层的定义通常由 `Implicit.g.cpp` 生成并转发到 implementation 层。

### 4.0.3 `*.cpp`（implementation 层）里典型实现方式

核心实现逻辑一般包括：

1. `AnimationsProperty()`：使用 `DependencyProperty::RegisterAttached` 注册附加属性
2. `GetAnimations(obj)`：`obj.GetValue(AnimationsProperty())`
3. `SetAnimations(obj, value)`：`obj.SetValue(AnimationsProperty(), value)`

你当前实现属于“标准形态”，其中关键点是：

- `RegisterAttached` 的 ownerType/targetType 一定要匹配你的使用场景
- 回调 `PropertyChangedCallback` 里通常 `try_as<UIElement>()` 或其他目标类型
- 若值类型是自定义 `runtimeclass`，常见用 `try_as<YourType>()` 取值

### 4.0.4 `*.g.cpp`（public 层转发）为什么必须参与编译

XAML 元数据代码（`XamlTypeInfo.g.cpp`）调用的是：

- `winrt::OpenNet::UI::Xaml::Media::Animations::Implicit::GetAnimations(...)`

而不是 `implementation::Implicit::GetAnimations(...)`。

public 层方法定义通常在生成文件（如 `OpenNet\\Generated Files\\UI\\Xaml\\Media\\Animations\\Implicit.g.cpp`）中，典型内容就是：

- `Implicit::GetAnimations(...) { return implementation::Implicit::GetAnimations(...); }`

所以：

- 只编译 `Implicit.cpp` 不够
- **必须**确保 `Implicit.g.cpp` 也被编译/链接

---

### 4.1 XAML 侧会怎样使用附加属性
当你在 XAML 中写类似：

- `animations:Implicit.Animations="..."`

XAML 引擎需要：

1. 找到声明类型 `Implicit`
2. 找到名为 `Animations` 的附加属性
3. 通过 `GetAnimations/SetAnimations`（或依赖属性 getter/setter）完成访问

因此 `XamlTypeInfo.g.cpp` 会生成类似：

- `GetAttachableMember_Animations<TDeclaringType, TargetType>`
- `SetAttachableMember_Animations<TDeclaringType, TargetType, TValue>`

最终会调用：

- `TDeclaringType::GetAnimations(...)`
- `TDeclaringType::SetAnimations(...)`

这里的 `TDeclaringType` 是 **public 投影类型** `winrt::...::Implicit`。

### 4.2 为什么需要 `Implicit.g.cpp`
public 类型 `Implicit::GetAnimations/SetAnimations` 的定义通常在 `Implicit.g.cpp`，它再调用 implementation。

所以当 `Implicit.g.cpp` 缺失/未编译时：
- implementation 层函数存在也没用
- `XamlTypeInfo.g.cpp` 依然找不到 public 层符号

---

### 4.3 XAML 的推荐写法（StaticResource / ThemeResource / 直接内联）

#### 4.3.1 在 `ResourceDictionary` 里定义动画集合（推荐）

把 `ImplicitAnimationSet` 放到资源里，再通过附加属性引用：

- `animations:Implicit.Animations="{StaticResource SomeImplicitAnimations}"`

优点：

- 多处复用
- XAML 更清晰

#### 4.3.2 StaticResource vs ThemeResource

- `StaticResource`：加载时解析一次，适合大多数动画资源
- `ThemeResource`：随主题切换可能重新解析，适合主题资源

动画一般选 `StaticResource`。

### 4.4 XAML 命名空间（`xmlns:`）与 C++ 命名空间（`namespace winrt::...`）如何对应

#### 4.4.1 `xmlns:animations="using:..."` 对应哪个命名空间

XAML 的 `using:` 指向的是 **WinRT 命名空间**（也就是你 IDL 里 `namespace ...` 的那一段）。

例如你的 IDL：

- `namespace OpenNet.UI.Xaml.Media.Animations { ... }`

那么 XAML 写：

- `xmlns:animations="using:OpenNet.UI.Xaml.Media.Animations"`

对应的 C++ 投影命名空间通常是：

- `winrt::OpenNet::UI::Xaml::Media::Animations`

注意：

- XAML 里命名空间用 `.` 分段；C++ 用 `::`。
- XAML 永远不会引用 `::implementation` 子命名空间。

#### 4.4.2 为什么 XamlTypeInfo 调用的是 public `Implicit` 而不是 `implementation::Implicit`

XAML 运行时只认识 WinRT runtimeclass（public 投影类型）：

- public：`winrt::OpenNet::UI::Xaml::Media::Animations::Implicit`
- implementation：`winrt::OpenNet::UI::Xaml::Media::Animations::implementation::Implicit`

因此 `XamlTypeInfo.g.cpp` 里会生成对 public 的调用，例如：

- `Implicit::GetAnimations(...)`

public 的实现定义一般在 `Implicit.g.cpp`，然后转发到 `implementation::Implicit`。

### 4.5 完整可复制的 XAML 示例（按 `Implicit.Animations` 用法）

> TimeSpan 的字面量格式在 WinUI/XAML 环境可能随项目设置略有差异；若 `OffsetDuration` 不能直接写字符串，请用你项目里已成功的写法或在代码里设置。

```xml
<Page
    ...
    xmlns:animations="using:OpenNet.UI.Xaml.Media.Animations">

    <Page.Resources>
        <animations:ImplicitAnimationSet x:Key="MainPageImplicitAnimations">
            <!-- contentproperty("Children"): 允许把子 Timeline 直接写在内部 -->
            <animations:OffsetAnimation OffsetDuration="0:0:0.25" />
        </animations:ImplicitAnimationSet>
    </Page.Resources>

    <Grid>
        <Border
            Width="200"
            Height="60"
            Background="Gray"
            animations:Implicit.Animations="{StaticResource MainPageImplicitAnimations}" />
    </Grid>
</Page>
```

---

## 5. 最重要的工程规则：生成的 `.g.cpp` 必须参与编译

### 5.1 你需要确保以下两点之一成立

#### 方案 A（推荐）：让项目自动把生成文件纳入编译
检查 `vcxproj`（或项目属性）里是否启用了 C++/WinRT 相关集成：

- 是否有针对 IDL 的生成步骤
- 输出的 `Generated Files` 路径是否被项目当作源文件集的一部分

不同项目设置名称可能不同，但核心要点是：

- 生成出的 `OpenNet\Generated Files\**\*.g.cpp` 必须被编译成 `.obj` 并参与链接。

#### 方案 B（你已经验证可行）：手动把 `*.g.cpp` 加进工程
例如把：

- `OpenNet\Generated Files\UI\Xaml\Media\Animations\Implicit.g.cpp`

加入项目的 `ClCompile`。

注意：
- 这是“救急”办法。
- 你后续新增更多 runtimeclass 时，仍可能不断遇到同样问题。

建议最终回到方案 A：让生成文件自动纳入编译。

---

## 6. 日常开发建议（避免再踩坑）

### 6.1 新增 WinRT 类型（尤其被 XAML 使用）时
建议流程：

1. 优先用 VS 模板/向导创建（能自动更新工程设置）
2. 新增 `.idl`
3. 编译一次，确认生成文件出现：
   - `Generated Files\...\YourType.g.h`
   - `Generated Files\...\YourType.g.cpp`
4. 确保这些 `.g.cpp` 在“解决方案资源管理器”中作为编译单元存在
5. 再开始写 implementation 逻辑

### 6.2 当你看到 LNK2019（unresolved external）时，如何快速定位
按顺序做：

1. 看错误里“缺的符号”属于谁
   - 如果是 `winrt::XXX::YourType::SomeStatic(...)`（public 层）
   - 大概率缺的是 `YourType.g.cpp` 的链接

2. 看错误里“referenced in”是谁
   - 如果是 `XamlTypeInfo.g.cpp`，说明是 XAML 元数据代码在调用

3. 搜索生成目录有没有 `YourType.g.cpp`
   - 有 → 但没编译/没链接
   - 没有 → 生成步骤没跑/IDL 没纳入生成

4. 确认 `YourType.g.cpp` 是否真的参与编译
   - 在 VS 输出窗口看是否编译了该文件
   - 或在中间目录（`$(IntDir)`）确认是否生成对应 `.obj`

### 6.3 什么时候 `XamlTypeInfo` 会引用你的类型
满足其中任意一条通常就会出现：

- 你的类型在 XAML 里作为控件/页面/资源使用
- 你定义了附加属性，并在 XAML 中使用
- 你实现了 XAML 需要的接口/模式（如 MarkupExtension 类似机制）

如果你写的是纯 C++ 逻辑类（不暴露为 WinRT 类型，不在 XAML 里引用），它一般不会进入 `XamlTypeInfo`。

---

## 7. 与 `pch.h`/预编译头相关的常见问题

你在 `XamlTypeInfo.g.cpp` 里看到大量 `static_assert( is_type_complete_v<...> ... )`，这类报错通常含义是：

- 生成代码需要完整类型定义
- 但是在当前编译单元中你只 include 了投影头，没 include implementation 头

解决方向通常是：

- 确保 `pch.h`（或当前 `.cpp`）包含了对应的实现头文件（例如 `MainWindow.xaml.h`、你自定义控件的实现头等）

`XamlTypeInfo.g.cpp` 自己不会“帮你 include”，它只会 assert 提醒。

---

## 8. Templated Control（无 code-behind 的自定义控件）最佳实践（基于 Microsoft Learn）

> 参考：Microsoft Learn《Build XAML controls with C++/WinRT》（WinUI 3 版本）。
>
> 这类控件的核心思想：**控件逻辑在 C++/WinRT（`*.idl/*.h/*.cpp`），外观完全在 XAML 模板（通常 `Themes/Generic.xaml` 或拆分的 ResourceDictionary）**。

### 8.1 “Custom Control (WinUI)”模板做了什么

按官方文档，使用 VS 的 **Custom Control (WinUI)** 模板添加控件时，通常会生成：

- `YourControl.idl`：声明 `runtimeclass YourControl : Microsoft.UI.Xaml.Controls.Control`
- `YourControl.h`：`#include "YourControl.g.h"`，并实现属性/事件/override
- `YourControl.cpp`：实现 DP 注册，并 `#include "YourControl.g.cpp"`（通常用 `__has_include` 宏防御）

关键点（和你这次链接问题相关）：

- `YourControl.g.cpp` 是 **public 投影层/激活工厂/转发** 的实现来源之一
- 如果它没被编译/链接，就可能出现 `LNK2019`（例如 public 静态方法、构造函数、激活入口缺失）

### 8.2 DefaultStyleKey 与 `Themes/Generic.xaml`

官方文档示例里，在构造函数设置默认样式键，例如：

- `DefaultStyleKey(winrt::box_value(L"Namespace.YourControl"));`

并在 `Themes/Generic.xaml` 中提供：

- `Style TargetType="local:YourControl"`
- `Setter Property="Template"`

注意：

- `Themes/Generic.xaml` 的文件名和目录（`Themes/Generic.xaml`）对于 WinUI 默认样式发现机制非常关键。
- 你项目目前把模板拆到 `*_ResourceDictionary.xaml` 再由 `Generic.xaml` 合并，这是常见且更可维护的做法。

### 8.3 依赖属性（DependencyProperty）的标准三件套（来自官方结构）

官方示例强调 DP 的声明模式：

1. IDL：
   - `static Microsoft.UI.Xaml.DependencyProperty XxxProperty{ get; };`
   - `Type Xxx;`
2. C++：
   - 在 `.h` 暴露 `Xxx()`/`Xxx(value)` 以及 `static XxxProperty()`
   - 在 `.cpp` 用 `DependencyProperty::Register(...)` 注册

这同样适用于你的附加属性/普通属性：

- 普通 DP → `Register`
- 附加属性 → `RegisterAttached`

### 8.4 “只写 ResourceDictionary（纯样式）”不会产生 `*.xaml.h/.xaml.cpp`

一个非常容易混淆、也最容易引发工程项配置错误的点：

- `ResourceDictionary.xaml`（纯样式/模板）**本身不需要 code-behind**
- 在这种情况下通常不会生成你期望的 `YourThing.xaml.g.h`（因为没有 XAML + code-behind 的那条链）

它的职责就是：

- 被 `Generic.xaml` 或 `App.xaml` 合并
- 提供 `Style`/`ControlTemplate`/`Resources`

不要期待它像 `Page.xaml` / `UserControl.xaml` 那样有 `*.xaml.h/.xaml.cpp`。

### 8.5 命名冲突陷阱：XAML-only 字典与 C++/WinRT 类型/文件同名

你项目的仓库规范里已经强调：

- `TitleCard` 是 **templated control（C++/WinRT Control）**
- 模板在 `TitleCard_ResourceDictionary.xaml`
- **不要**创建 `TitleCard.xaml`（否则会引入一条完全不同的 XAML code-behind 生成链）

这里把“为什么”讲清楚：

1. 当你存在 `TitleCard.xaml` 并且项目把它当成 XAML 控件来编译时
   - 会生成 `TitleCard.xaml.g.h` / `TitleCard.xaml.g.cpp`
   - `XamlTypeInfo.g.cpp` 会把它当作可激活/可反射访问的 XAML 类型

2. 如果你同时又有 `TitleCard.idl`/`TitleCard.h`/`TitleCard.cpp`
   - 两套体系会在“类型名、include、生成文件路径”上发生碰撞
   - 轻则出现错误 include（例如 `*.g.h` 条件 include 到了 `TitleCard.xaml.g.h`）
   - 重则出现重复类型/符号、或者 `XamlTypeInfo.g.cpp` 以错误方式注册本地类型

3. 你贴的 `TitleCard.g.h` 里就有一个典型的“防御式 include”逻辑：

- 如果存在 `UI/Xaml/Control/Card/TitleCard.xaml.g.h`，生成头可能会尝试 include 它（受宏或 `__has_include` 影响）

因此命名建议：

- 纯样式字典：用 `*_ResourceDictionary.xaml` 或 `*.Styles.xaml` 这种不会与类型名冲突的命名
- 不要创建与 `runtimeclass` 同名的 `*.xaml`（除非你确实要做 `UserControl`/`Page` 这种带 code-behind 的类型）

### 8.6 “不用模板也能生成 stub”的官方流程（以及在本仓库的含义）

官方文档也给了一个“手动流程”：

1. 先添加 `.idl`
2. Build 一次让 midl 生成 `winmd` 和 `Generated Files/sources` 下的 stub
3. 把 stub 的 `.h/.cpp` 复制进项目目录并 **Include In Project**
4. 注意 stub 可能带 `static_assert` 防止直接编译，需要替换为真实实现

在本仓库里，这个流程通常意味着：

- 你必须确保 **复制出来的 `.h/.cpp`** 和 **对应的 `*.g.cpp`** 都在工程里参与编译
- 否则会重现你这次遇到的：`XamlTypeInfo.g.cpp` 引用 public 符号 → 链接找不到

---

## 9. 推荐的项目约定（可团队化）

1. **所有 WinRT/XAML 相关类型必须由模板创建或确保生成文件自动编译**
2. 生成目录（如 `OpenNet\Generated Files`）只读，不手改
3. 一旦出现 LNK2019：
   - 优先检查“是否缺某个 `.g.cpp` 的编译/链接”
4. 对附加属性：
   - 确保 public `runtimeclass` 的对应 `*.g.cpp` 编译存在

---

## 10. 速查清单（Checklist）

- [ ] `*.idl` 是否纳入项目并触发生成？
- [ ] `Generated Files` 下是否出现对应 `YourType.g.cpp`？
- [ ] `YourType.g.cpp` 是否被编译产生 `.obj`？
- [ ] 链接错误缺的是 public 还是 implementation 层符号？
- [ ] 如果 `XamlTypeInfo.g.cpp` 引用你的类型，`pch.h` 是否 include 了对应实现头？

---

## 11. 你这次问题的“最短总结”

- `XamlTypeInfo.g.cpp` 调用的是 `winrt::...::Implicit::GetAnimations/SetAnimations`（public 层）。
- public 层函数定义在生成文件 `Implicit.g.cpp` 里。
- `Implicit.g.cpp` 生成了但没有编译/链接 → LNK2019。
- 把 `Implicit.g.cpp` 加入编译后，public 层符号可用 → 链接通过，运行正常。
