# WinUI 3 (C++/WinRT) Attached Properties 开发指南（本仓库实践）

> 适用范围：`OpenNet`（WinUI 3 + C++/WinRT）。
>
> 本文针对这次问题：应用启动期出现 `0xC000027B`（fail-fast）或 `0x802B000A`/`0x80004005`，表现为“内部错误/未指定错误”，最终定位为 **AttachedProperty 实现方式 + 模板绑定方式**不符合 WinUI 3 预期。

---

## 1. 症状与典型日志

常见现象：

- 启动时直接崩溃，输出窗口只有
  - `Microsoft.UI.Xaml.dll ... 80004005 - Unspecified error`
  - `WinRT originate error - 0x802B000A : 'The text associated with this error code could not be found.'`
  - 甚至直接 `0xC000027B` + `0xC0000602 fail fast`

当错误能被显示出来时，常见可见错误为：

- `The property 'SelectedIcon' was not found in type 'OpenNet.Helpers.NavItemIconHelper'.`
- `Cannot find a Resource with the Name/Key MainNavigationViewStyle`（排查时移除字典导致）

---

## 2. 根因（重点）

### 2.1 Attached DependencyProperty 的注册方式

在 WinUI 3 / XAML 运行时中，AttachedProperty 的元数据必须在 XAML 使用它之前就稳定可用。

**推荐且稳定的模式（C# 参考一致）：**

- `DependencyProperty::RegisterAttached(...)` 只执行一次
- 用“静态字段”保存 DP
- `XxxProperty()` getter 只返回该静态字段

反例（容易埋雷）：

- 在 `XxxProperty()` 方法里每次都 `RegisterAttached(...)`（或依赖首次访问时注册）

这种模式在某些情况下会导致 XAML 对 `Type.StaticMember` 的解析时序不稳定，表现为：

- 有时是“找不到属性”
- 有时直接 fail-fast（看上去像内部错误）

本仓库的正确实现参考：`OpenNet/Helpers/NavItemIconHelper.*`。

### 2.2 ControlTemplate 内绑定 AttachedProperty 的写法

在模板里访问 attached property：

✅ 推荐：

```xaml
{Binding (helpers:NavItemIconHelper.SelectedIcon), RelativeSource={RelativeSource Mode=TemplatedParent}}
```

❌ 避免：

```xaml
{TemplateBinding helpers:NavItemIconHelper.SelectedIcon}
```

原因：`TemplateBinding` 对 attached property 的支持不稳定，容易导致运行时将其当作普通属性解析，进而报“属性不存在”。

---

## 3. 标准实现模板（C++/WinRT）

以 `SelectedIcon` 为例：

### 3.1 `*.h`

- 声明 DP getter
- 声明私有静态字段

```cpp
struct NavItemIconHelper : NavItemIconHelperT<NavItemIconHelper>
{
    static Microsoft::UI::Xaml::DependencyProperty SelectedIconProperty();
    static Windows::Foundation::IInspectable GetSelectedIcon(Microsoft::UI::Xaml::DependencyObject const& obj);
    static void SetSelectedIcon(Microsoft::UI::Xaml::DependencyObject const& obj, Windows::Foundation::IInspectable const& value);

private:
    static Microsoft::UI::Xaml::DependencyProperty s_selectedIconProperty;
};
```

### 3.2 `*.cpp`

- **在命名空间作用域内初始化静态字段**，注册一次

```cpp
Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::s_selectedIconProperty =
    Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
        L"SelectedIcon",
        winrt::xaml_typename<winrt::Windows::Foundation::IInspectable>(),
        winrt::xaml_typename<class_type>(),
        Microsoft::UI::Xaml::PropertyMetadata(nullptr));

Microsoft::UI::Xaml::DependencyProperty NavItemIconHelper::SelectedIconProperty()
{
    return s_selectedIconProperty;
}
```

---

## 4. 排查流程（项目内实战）

当出现 fail-fast 且看不到具体 XAML 行号时：

1. **先做资源级二分**：
   - 临时注释 `App.xaml` 的某个 `MergedDictionaries`，看是否能启动。
   - 注意：如果 `MainWindow.xaml` 依赖某个 Style Key（例如 `MainNavigationViewStyle`），移除对应字典会导致“缺资源”而无法继续排查。

2. **确定字典后，转为字典内部二分**：
   - 保留关键 Style Key 的定义（避免缺资源）
   - 将其 `Template` 暂时替换为最小可运行版本
   - 逐段恢复模板内容定位到具体触发元素

3. 针对附加属性相关的错误：
   - 先检查 attached DP 是否为“静态字段一次性注册”
   - 再检查模板内是否用了 `TemplateBinding` 读取 attached property

---

## 5. 与本仓库文件的关系

- AttachedProperty 实现：`OpenNet/Helpers/NavItemIconHelper.idl/.h/.cpp`
- 引用点（XAML）：`OpenNet/Styles/NavigationView.xaml`
- 全局合并：`OpenNet/App.xaml`
- 使用样式的页面：`OpenNet/MainWindow.xaml`（`MainNavigationViewStyle`）

---

## 6. 最佳实践清单

- ✅ Attached DP：用静态字段一次性注册
- ✅ `RegisterAttached` 的 owner type：用 `xaml_typename<class_type>()`
- ✅ 模板读取 attached property：`Binding (Type.AttachedProperty) + RelativeSource=TemplatedParent`
- ❌ 不要在 DP getter 中做 Register（避免时序问题）
- ❌ 不要用 `TemplateBinding` 读取 attached property

