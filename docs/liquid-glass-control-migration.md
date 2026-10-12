# OpenNet 材质与文字外观架构

## TextBox 输入焦点修复（2026-10-17）

MaterialTextBoxTemplate 的实际编辑器曾固定设置 IsTabStop=False。在 WinUI 中这也禁止 Focus() 获取焦点，外层业务 TextBox 的模板又没有自己的 ContentElement，因此玻璃模式下无法输入。现在内部 LiquidGlassTextBox 通过 TemplateBinding 跟随业务控件的 IsTabStop；IsReadOnly、InputScope 和原有文本/选择双向同步不变。

玻璃 TextBox 连接期间，业务控件使用 TabFocusNavigation=Once，避免内外两个 TextBox 产生重复 Tab 停点或 Shift+Tab 回到业务控件后又被转交回来。断开连接时恢复原来的本地值、绑定或样式来源。对已经获得焦点的 TextBox 应用玻璃模板时也会排队转交焦点，仍执行连接状态、IsLoaded、XamlRoot 和当前焦点检查，避免导航后抢回焦点。普通材质的模板和焦点设置保持不变。

验证：16 项材质/焦点源码检查通过，新增检查覆盖编辑器可聚焦绑定与导航设置恢复。OpenNet.slnx 的增量编译停在依赖项目解析阶段、长时间无进展后已停止（artifacts/material-textbox-input-fix.log）；本轮没有完成编译或运行验证，不能据此宣称中文输入法、粘贴及 Tab 导航已经实测通过。

## 焦点与布局回归修复（2026-10-15）

用户提供的首次异常落在 MaterialControlBridge 的 GotFocus 转交，实际调用 Focus(Unfocused)；WinUI 的 UIElement::FocusImpl 明确返回 E_INVALIDARG。GotFocus 是延迟分发的事件，导航或材质模板替换之后，不能把事件名当作当前仍持有焦点的证明。

转交现在排队执行，并重新检查连接 Active、源和目标 IsLoaded、相同且有效的 XamlRoot、源的非 Unfocused 状态，以及 FocusManager 当前仍指向源。旧连接在替换模板前失效。可选焦点转交只容忍 ERROR_NOT_FOUND / RO_E_CLOSED 两种断开竞态，不设置全局 UnhandledException.Handled，也不吞掉其它错误。

TasksPage 的内联搜索专门使用 36 DIP 的相同原生/玻璃高度、居中布局，以及 RestScale / FocusedScale=1。库默认的 56 DIP 最小高度与 0.8 idle scale 不适合工具栏；其它页面和预览仍保留库默认值。排序表头保持横向 Stretch，但移除按钮的默认最小高度，纵向居中并禁止标题换行，按文字自然高度测量而非硬编码整个 Header 高度。

胡桃本轮补齐 IAdvancedCollectionView.CurrentItem 的接口 setter（实现类早已有 setter 并调用 MoveCurrentTo），保留 TwoWay 选择语义；静态成员从静态类型绑定；LayoutSwitch.Current 使用公共 DP 标识符。首页 MaterialCardButton 改为无额外表面的 ContentControl 宿主，内部保留真实库按钮；移除 Announcement 卡片模板的旧背景框，原生表面使用非 Composition 的独立主题画刷，玻璃表面的 XAML 与 shader 使用同一圆角。命令等属性用 DP 回调传递，不依赖反射 Binding，材质订阅按 Loaded/Unloaded 注册撤销。

本轮新增 focus/layout 源码回归检查，合计 20 项源码检查通过。OpenNet.slnx 的 Debug/x64 增量构建为 0 错误、27 警告，记录见 artifacts/material-focus-layout-fix.log。编译验证不替代运行验证，尤其应复测“玻璃→原生→玻璃→RSS→任务页”、键盘焦点、表头排序、首页卡片鼠标移动/放大与快速切换。

2026-10-16 胡桃主项目通过 Snap.Hutao.Remastered.slnx /t:Snap_Hutao_Remastered 的 Debug/x64 增量构建：0 错误、553 警告，日志 artifacts/hutao-userview-bindings-verified.log。本轮保持业务 x:Bind，修正静态路径、类型回退及 UserView 的 Uri/昵称转换，不以修改生成文件或降级全部绑定绕过错误。两个应用均未启动；警告和运行时画面不在“编译通过”的保证范围内。

## 当前实现：单业务控件与真实库视觉模板（2026-10-15）

页面只声明原生业务控件，保留其 x:Name、x:Bind、Loaded 初始化及业务事件，并设置 ouxct:ControlAppearance.IsEnabled="True"。ControlAppearance 切换该实例的 Template，而非替换业务对象；关闭材质或进入高对比度时恢复原来的本地模板/绑定/样式模板。

MaterialLibraryTemplates.xaml 集中定义 11 种模板，每种模板内部只有一个真正的 wl:LiquidGlass 控件。Slider / ToggleSwitch 仍由库的 Kube 视觉模型管理镜片、实体层、弹簧和指针，不用旧 MaterialTemplates.xaml 的画刷近似。旧字典已退出项目编译，仅保留历史源码。模板不覆盖库的 Background、CornerRadius 或内部 Template，也不把原生 Slider.Foreground 强加给库的进度层。

MaterialControlBridge 的模板连接独立于旧 SwitchPresenter 连接：

- 首次始终从业务对象向视觉传值，绝不能以可见分支为由把库默认值写回业务对象。
- 多属性同步共享重入保护；先设置范围/项目来源，再连接 Value / SelectedIndex。视觉暂时出现的 SelectedIndex=-1 不回写，业务对象仍可主动清空选择。
- ComboBox 共享数据源与 ItemTemplate / ItemTemplateSelector；显式 Items 也只由当前模板呈现，不复制第二套配置树。类型选择器使用带 x:DataType 的 ItemTemplate 与 x:Bind Name，不依赖 DisplayMemberPath 反射。
- Button / HyperlinkButton 的点击通过原生 Invoke 契约执行原业务控件，Click、Command 和 Flyout 不在内层再配置一次。选择类控件通过 IsChecked 触发原业务值事件。
- 键盘焦点进入业务对象时转交真实库视觉。模板换回前撤销该连接的属性与焦点订阅；库视觉自行处理 Loaded/Unloaded 和动效资源。
- 已移除 344 处页面材质双分支，包括设置模板和 Guide 卡片预览。仍保留三处 SearchBox 以及一个有独立 Click 业务的 CheckBox 适配器：AutoSuggestBox 的 QuerySubmitted / SuggestionChosen 不能从外部安全重发；Choice 的 Click 不等同于值变化。它们不是模板能力缺失，而是尚未完成的事件适配边界，不能用丢弃事件的方式强行折叠。

机械迁移入口为 scripts/collapse-material-presenters.mjs --library-templates --write；未加 --write 时只盘点。该模式只处理已知原生/玻璃两分支，保留布局、绑定和原名，拒绝上述事件边界。不要再次运行 restore-library-material-controls.mjs 把已经迁移的页面恢复为双树。

这仍不是任意主题插件加载器。后续主题包应以稳定业务控件契约提供模板资源，并明确支持的视觉/事件能力；不能仅加载另一套画刷就宣称与库的几何、交互动效等价。视觉、键盘、无障碍和快速反复切换仍需运行验证，源码检查与编译不能代替这些验证。

背景类型保存入口另行拒绝越界/空选择，防止瞬态 UI 值污染 SQLite；不会猜测并覆盖用户已经保存的设置。

模板的文本选区通过 SelectionChanged / Select 同步，而不是访问不存在的公开 SelectionStartProperty / SelectionLengthProperty。转交选区前先同步文本，允许编辑器先通知选区再通知 TextChanged。密码视觉将焦点交给 InnerPasswordBox；RadioButton 的视觉分组与业务分组分离，未显式指定 GroupName 时保持父容器局部范围。

本轮检查：16 项源码回归检查通过。构建只调用 OpenNet.slnx /t:OpenNet /p:Configuration=Debug /p:Platform=x64 /m:2 /nr:false，不使用 Clean / Rebuild、额外产物目录或启动应用。最终增量记录见 artifacts/material-library-templates-complete.log。运行时的几何、动效、无障碍、输入与快速切换尚未验证。

胡桃先前修复三处 ABI IID 的 ref 生命周期：返回 static readonly Guid 字段的引用，字节序与原始 16 字节值一致。附带修正 Flyout 根 x:Bind、缺失的 15 个液态玻璃资源键、整数 FallbackValue，以及动态主题颜色/可空颜色绑定。artifacts/hutao-net11-final.log 是先前失败记录，其中静态成员访问、IAdvancedCollectionView.CurrentItem setter 和公开 DP 标识符的问题已继续从源 XAML/C# 修复；不能把旧日志当作当前结果。2026-10-16 的后续修复还覆盖设置、测试、攻略页的静态命令和列表、Cultivation 条目 Flyout 的类型上下文及 FilterTokens 集合包装契约。未手改生成代码内容；确认为过期且阻断预编译的少量生成文件逐文件备份后由编译器正常再生成，没有清空 obj/bin。

以下“页面双分支”内容记录先前实现，除上述四处适配边界外，不再是新页面的架构建议。

## 实际控件，而不是画刷近似

液态玻璃分支必须实例化真实的 wl:LiquidGlassButton、LiquidGlassSlider、LiquidGlassToggleSwitch 等库控件。只为原生控件替换 ControlTemplate 和 Background 不能复制库的视觉模型：Slider 的 90×60 光学镜片、白色实体层、开关的 authored geometry / Viewbox、指针场和弹簧都由控件的 C++ 实现负责，不属于 HLSL 画刷本身。

此前“单个原生控件 + MaterialTemplates”方案把这些效果丢掉了，现已恢复真实控件分支。页面通过 essential:SwitchPresenter 选择原生契约控件或实际库控件，保留旧 x:Name / x:Bind / Command 及事件方法。新配置窗口的原生编辑器不参与材质切换，旁边的预览使用真实库控件。

- MaterialControlState：维护全局模式、类型开关及已加载画刷的参数刷新。
- LiquidGlassProfile / LiquidGlassParameter：保存每种类型的独立参数与默认值。
- MaterialControlBridge：集中同步 IsOn、IsChecked、Value、SelectedIndex、Text、Password；隐藏的原生契约仍供原有业务字段读取。值事件经原生属性变化触发，不重复挂到两个分支。不可继承自原生类型的开关、密码和搜索包装控件，其 Loaded 初始化通过代理调用原来的原生契约初始化方法。
- 切换容器只显示一支；不在代码中从一个控件偷走 UIElement 内容。普通按钮内容只有非 UIElement 对象变化时才允许同步。
- FlyoutBase 可以共享：玻璃按钮引用原有按钮的 Flyout，不复制弹出层里的设置树、命名元素和绑定。
- InfoBar.ActionButton 必须是 ButtonBase，使用返回真实 Button / LiquidGlassButton 的 x:Bind 工厂，不能把 SwitchPresenter 塞入该属性。
- 保留 essential / wl / ouxct 命名空间前缀，业务绑定继续优先 x:Bind。
- 透明表头按钮保留原生样式；玻璃分支不套用原生 Style / Template / Background 来覆盖库自己的视觉实现。显式尺寸直接传给库控件，让它的 Viewbox 缩放，而不是只限制宿主再裁切镜片。

SwitchPresenter.Value 是 Object。延迟加载场景使用 BoxInt32(Current.TypeMaterial) 函数绑定，以规避 XAML 编译器把 int32_t 直接赋给 IInspectable 延迟缓存的已知问题，和 TaskFilesPage.Tag 的既有 workaround 一致。

维护脚本 scripts/restore-library-material-controls.mjs 用于机械迁移，不修改 Generated Files。迁移后需要处理共享 Flyout、受类型限制的属性、原生契约初始化和格式；不要再次运行 collapse-material-presenters.mjs 把真实库控件折叠成原生画刷近似。

### 当前双分支的限制

SwitchPresenter 不是主题切换的必要条件；它只是当前恢复库效果的过渡实现，不代表页面内重复两套控件的架构已经完成。相同控件契约可以通过 Style / ControlTemplate 切换视觉，但还必须保留库控件负责的交互和视觉模型，不能只替换 Background。

当前库的 LiquidGlassToggleSwitch 继承 ToggleButton，LiquidGlassPasswordBox / LiquidGlassSearchBox 继承 ContentControl，而非原生 ToggleSwitch / PasswordBox / AutoSuggestBox，不能靠套用模板变成这些类型。后续应把差异集中在可复用材质适配控件中，统一公开业务属性、事件、命令和 helper 目标；内部再管理视觉切换与生命周期，让页面只有一份业务绑定。此方案仍需实现，不能把现有 MaterialControlBridge 等同于完成了这一封装。

## 材质表面与生命周期

真实库控件自行拥有 GlassBrush。MaterialControlState 按实际运行时类型选取参数 profile；不再把它们误判为 Card，也不改写其几何、DPI、白色实体层或交互动效。Slider / ToggleSwitch 参数默认值恢复库本身的 TintOpacity，不再强制 .9 的原生小旋钮补偿。卡片与现有 SettingsCard / CommandBar 的自有表面仍各自创建画刷，不使用全局共享 Composition 画刷。

桌面应用不能订阅 Windows.UI.ViewManagement.AccessibilitySettings.HighContrastChanged；微软将其列为[桌面不支持的事件](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/winrt-api-desktop-app-support#events)。ControlAppearance.IsEnabled 的属性回调只连接本地 Loaded/Unloaded，不能在 XAML 赋值期间创建系统监听。模板应用和通知注册在 Loaded 后由 UI Dispatcher 延迟执行。

高对比度通知通过所属 UI 线程窗口的 Win32 subclass 监听 WM_SETTINGCHANGE / WM_THEMECHANGED / WM_SYSCOLORCHANGE，状态查询使用 SPI_GETHIGHCONTRAST。不再使用 ThemeSettings.Changed 或其 event_revoker：在 DispatcherQueue 关闭期间撤销该 SDK 事件曾导致 Microsoft.UI.dll 内部 terminate，不能靠外层 try/catch 修复。

窗口监听与控件卸载解耦：页面导航、材质模板切换和 Unloaded 只更新控件跟踪；WM_NCDESTROY 移除窗口 hook 和纯标量记录。应用 ReallyClose 在后台引擎关闭、Window.Close 和 Application.Exit 之前调用 ControlAppearance.Shutdown；DispatcherQueue.ShutdownStarting 另有队列自身持有的无捕获回调作兜底，不使用 auto_revoke。退出后停止新观察器、排队模板应用和画刷更新。状态查询失败时保留原生样式；hook 安装失败时记录诊断并在后续刷新时继续查询状态，不吞掉真正的模板或画刷错误。

模板中指向自身附加画刷属性的少量 Binding 是有意保留的：MaterialTemplates.xaml 是没有 x:Class 的纯模板字典。这些绑定不涉及业务 ViewModel，也不在页面之间同步控件。业务使用点及有代码支持的设置模板仍优先 x:Bind。

## 设置与持久化

主题设置的两张卡片分别打开 LiquidGlassControlsWindow 和 LiquidGlassEffectsWindow。它们是独立的 WinUI3Package.WindowEx，使用 WindowExBase 参与现有窗口主题、背景及位置管理；重复点击会激活已有窗口。

- 控件窗口选择全局材质，并通过 ToggleSwitch 启用各控件类型；预览遵循真实全局材质和类型开关。
- 效果窗口选择一个 LiquidGlassProfile，以 Slider 和 x:Bind TwoWay 编辑其 LiquidGlassParameter.Value。预览始终启用玻璃，不改变应用的启用状态。编辑器保持原生外观。
- 14 类控件独立配置：Button、ToggleButton、HyperlinkButton、CheckBox、RadioButton、Slider、TextBox、SearchBox、PasswordBox、ComboBox、ToggleSwitch、Card、FloatingPanel、TabBar。AutoSuggestBox 内部输入表面归属 SearchBox，不混入 TextBox。
- 30 项参数以库 LiquidGlassPresets.CreateBrush 的实际属性为默认值；真实库控件自行管理几何和白色实体层，不叠加原生小旋钮补偿。默认面板不再强制套用深灰 .18 染色或统一的高级参数。
- 参数修改自动保存到 SQLite 分类 liquid_glass_profiles_v1，键为 Type.Parameter，例如 Button.RefractionStrength。恢复单项/当前类型默认值删除对应覆盖，回到预设；不会修改其他类型或启用开关。非法/非有限数据不会传入 Composition。
- 原 liquid_glass 分类的类型开关和 material_style 继续沿用。旧全局光学值保留在数据库但不再参与渲染，避免它们继续覆盖每种控件的默认预设；新配置不会删除旧记录。
- 每次应用先恢复该画刷的初始光学参数快照，再写入当前类型的覆盖；库控件的几何槽位由库管理，不从旧快照覆盖。UI Dispatcher 合并刷新；重复加载、主题切换和调节不会累乘参数。
- VisualMaterials 的旧资源键保留为普通主题画刷，不再保存全局 LiquidGlassBrush。设置卡片、Border 表面和 CommandBar 通过 Surface 和自身 Brush 绑定创建独立画刷，且保留已有模板和交互。

扩展一个参数：在 LiquidGlassOptics.Descriptors 增加范围、步进、读写函数，并补充本地化资源；参数快照随描述表自动扩展。不要在页面另写全局参数；不要让两个视觉元素复用同一 Composition 画刷。

新增 XAML/IDL 类型时，vcxproj 中的实现头文件必须配置 DependentUpon，关联其 XAML 或 IDL，供 XAML 编译器生成实现头文件包含列表。LiquidGlassProfile.h 同时声明 Profile 与 Parameter，两者共享同一 IDL 关联；不要手改 XamlTypeInfo.g.cpp 的 include 或工厂表。

2026-10-13 编译验证：使用 MSBuild OpenNet.slnx /t:OpenNet /p:Configuration=Debug /p:Platform=x64 /m:2 /nr:false，不使用 Clean/Rebuild 或额外输出路径。最终源码增量构建为 0 错误、0 警告，日志 artifacts/liquid-glass-profiles-verified.log。前一轮资源重新生成仍报告项目已有的 PRI249（WebUI 资源名称限定符）及 mvvm_diagnostics 告警；本次未修改这些无关内容。未启动应用，视觉效果、交互及跨进程持久化恢复尚未做运行验证。

动态删除确认对话框的 RadioButton 也使用真实库控件切换并同步原生 IsChecked。旧 CreateTextBoxPresenter 兼容入口目前没有业务调用点，新界面不应使用其原生模板近似路径。

GuideView 的卡片预览也恢复真实 LiquidGlassCard 分支；InfoBar.ActionButton 使用返回 ButtonBase 的工厂。窗口的 Mica/Acrylic/自定义背景设置继续由原有窗口服务管理，本次没有改变这些设置。

## 字体大小

字体设置页通过 x:Bind 直接修改 Typography.Current.Scale，范围 0.8–2.0，步长 0.05；支持预览和重置为 1.0。设置保存于 typography/scale。

TypographyResources.xaml 定义默认文字层级；BaseFontSize 是未缩放的 DIP 基准。已加载文字立即更新，新加载文字读取当前比例，重复加载不累乘。显式字体尺寸优先于默认层级；图标字号不作为正文缩放。不使用整页 Viewbox 或 RenderTransform，不关闭 IsTextScaleFactorEnabled，所以 Windows 无障碍文字缩放仍由平台处理。

自定义命名文字样式应继承现有层级，或设置 ouxct:Typography.BaseFontSize。第三方控件若完全自行绘制文字，需要自行接入，不能承诺其私有模板自动缩放。

参考：[文字缩放](https://learn.microsoft.com/en-us/windows/apps/develop/input/text-scaling)、[无障碍文字要求](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/accessible-text-requirements)。

## XAML 编译器边界

- x:Bind 显式保留 OneWay / TwoWay，避免默认 OneTime 改变行为。
- ControlTemplate 的附加属性路径使用 (ContentControl).(ouxct:MaterialControlState.SettingValue)。
- TaskFilesPage 的延迟加载 ComboBox 使用 BoxInt32(FileIndex) 绑定 Tag，让生成代码的延迟缓存接收 Object，而不是直接把 int32_t 赋给 IInspectable。
- 之前命名元素 TwoWay 回写使用 GetDataRoot().NamedElement()、正向读取使用 get_self<implementation>() 的不一致仍是编译器边界。当前材质分支通过 MaterialControlBridge 的属性回调同步，不新增元素间 TwoWay x:Bind 镜像；原有业务中的元素间 TwoWay 仍可能需要 IDL 声明。
- 不修改 Generated Files；不通过修改模板部件名称规避生成错误。
- 供 Style.Setters 使用的附加 DP 必须在解析样式之前完成注册，不能仅等待 Get/Set 访问器首次调用。Typography.BaseFontSize 使用函数内静态缓存保持单次注册，但 App 构造阶段会显式调用 EnsureProperties；ControlAppearance 的样式/绑定属性同样预注册。C++ 生成的 XamlUserType.RunInitializer 是空实现，不会替我们执行这些懒 getter。未注册属性可在 OptimizedStyle 的 Setter 解析阶段返回 E_FAIL，再被外层包装成 Cannot create instance，而不提供具体属性名。

## 维护与验证

升级 WinUI 时核对原生模板契约和主题资源，来源许可见 material-templates-notice.md。新增控件首先保持原生行为，只给模板中的视觉表面接入独立画刷。

本次机械迁移脚本 scripts/collapse-material-presenters.mjs 默认只列出可迁移项，--write 才修改源 XAML；不会触碰生成目录或执行编译。

编译验证必须对 OpenNet.slnx 调用 MSBuild，保持 Debug/x64 等现有配置，不覆盖输出目录，不单独编译 vcxproj，不执行 Clean/Rebuild。编译成功不等同于运行验证：仍需在应用中检查连续切换材质、导航返回、输入焦点、开关事件只触发一次、高对比度，以及文字放大后的重排。

本轮验证（2026-10-13）：通过 OpenNet.slnx 的 OpenNet 目标完成 Debug/x64 增量编译，0 个错误、14 个警告。日志为 artifacts/material-appearance-build.log。未运行应用或烟测；不要把编译通过视为已验证视觉效果。纯状态类 Typography 与资源字典 TypographyResources.xaml 使用不同名称，避免误触发 XAML 代码支持类型推断；新增实现头通过项目 DependentUpon 元数据接入 XamlTypeInfo。

启动异常修正（2026-10-13）：移除桌面不支持的 HighContrastChanged 订阅，并将系统通知/模板应用移出 XAML 属性赋值回调；每窗口监听在 Loaded 后的 UI 调度中建立。OpenNet.slnx / Debug / x64 增量编译通过，0 个错误、0 个警告，日志为 artifacts/material-highcontrast-fix-build.log；尚未运行验证。

关闭异常修正（2026-10-13）：OpenNet.exe.4616.dmp 为 0xC0000409 / FAST_FAIL_FATAL_APP_EXIT，用户栈明确落在 Unloaded → PruneThemeObservers → ThemeObserver 析构 → ThemeSettings 事件撤销。现已移除该 WinRT 撤销链并改为窗口消息生命周期管理。本次依用户要求未编译、未运行测试，仅做源码与 dump 检查。

## 2026-10-13 真实控件回归修正

OpenGlassControls / OpenGlassEffects 缓存的是 window.Window() 返回的底层 Microsoft.UI.Xaml.Window。WindowEx 实际继承 ContentControl，并不实现 IWindow；不能对 WindowEx 调用 as<Microsoft.UI.Xaml.Window>()。项目现有 WindowHelper 使用的也是 WindowEx.Window()。

本轮修正通过 OpenNet.slnx Debug/x64 编译，0 错误；日志 artifacts/liquid-glass-real-controls-final-verified.log。存在既有 PRI249 等资源告警，未改动相关 WebUI 文件。未启动应用，窗口打开、完整交互、视觉效果与关闭仍需运行验证。

## 2026-10-14 原生 helper 的目标类型

附加属性并不一定接受任意 DependencyObject。AutoSuggestBoxHelper.SetAcrylicWorkaround 的参数和回调都要求 Microsoft.UI.Xaml.Controls.AutoSuggestBox；LiquidGlassSearchBox 继承 ContentControl，不能直接赋这个附加属性，否则在解析 XAML 时失败并被包装为 0x802B000A。

原生分支保留 AcrylicWorkaround；玻璃分支在 MaterialControlBridge 中将其应用到 InnerAutoSuggestBox，并检查是否已经启用。迁移脚本通过 material-helper-targets.mjs 过滤不兼容属性，tests/material-helper-targets.test.mjs 覆盖过滤规则及应用 XAML。LiquidGlassComboBox 和 LiquidGlassSlider 继承相应原生类型，保留其 helper，不做无差别删除。
