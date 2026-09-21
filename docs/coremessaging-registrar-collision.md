# CoreMessagingXP registrar 启动崩溃调查（2026-09-21）

## 已捕获的错误

在 `ca3c20f96fa71c8244a38db884df69478ea5dce1` 工作区的已部署 x64 Debug 包中，
使用 CDB 保持 MSIX 身份启动，在实际加载的
`System::Runtime::InteropServices::Marshal::ThrowExceptionForHR(int)` 入口断下：

```text
ECX = D0000035
AlpcServerAdapter::Create$+0x104
AlpcServerHost::Create$+0xd1
CuiRegistrar::Run+0xd7
CoreUIRunRegistrarServer+0xbc
RegistrarHost::RegistrarThreadProc+0x7b
```

这是 `HRESULT_FROM_NT(STATUS_OBJECT_NAME_COLLISION)`；原始 NTSTATUS 为
`0xC0000035`。不能把它的低 16 位当成 Win32 错误码。`0xC0000602` 是后续
fail-fast 的异常码，不是 ALPC 创建失败的原因。

另一次完整启动跟踪直接记录了 `NtAlpcCreatePort`：

| 顺序 | 调用者 | 端口 | 返回 NTSTATUS |
| --- | --- | --- | --- |
| 1 | WindowsAppRuntime 包内 CoreMessagingXP | `\BaseNamedObjects\CoreMessagingRegistrar-27356:00000000-0000-0000-0000-000000000000` | `00000000` |
| 2 | 应用 AppX 目录内 CoreMessagingXP | 同一个端口 | `C0000035` |

PID 27356 属于该次调试进程。端口名包含 PID，证据指向同一进程的两个模块实例，
不是另一个 OpenNet 实例占用了固定的跨进程端口。

## 两份相同 DLL 也会冲突

当前 Debug 进程同时加载：

```text
C:\Files\OpenNet\x64\Debug\OpenNet\AppX\CoreMessagingXP.dll
C:\Program Files\WindowsApps\Microsoft.WindowsAppRuntime.2_2.5.1.0_x64__8wekyb3d8bbwe\CoreMessagingXP.dll
```

两份文件版本都是 `10.0.27200.1054`，SHA-256 也相同：
`049A1379592513D05BADA39D43A46124833C2E5F6EC2EEC299927339E12D57DF`。
独立模块实例的内部单例状态不共享，但所创建的命名 ALPC 端口共享对象命名空间。
因此第一次初始化成功，另一个模块仍会尝试创建同名 registrar。

实际 XAML、XAML.Controls 和 dcompi 来自 framework package；部分原生依赖却来自
应用目录。仅对齐 NuGet 版本不能解决这种混合部署。

## 构建根因

OpenNet 是 framework-dependent MSIX，引用 `Microsoft.WindowsAppSDK.Runtime`。
Toolkit 的 `XamlToolkit.WinUI.ModuleBuilder` 是只提供 C++ 投影的静态库，
使用 split packages，没有 Runtime package reference。

MSVC 将 `ConfigurationType=StaticLibrary` 映射到 `OutputType=staticlibrary`。
`Microsoft.WindowsAppSDK.Base` 的 native targets 使用以下默认条件：

```xml
<PropertyGroup Condition="'$(MicrosoftWindowsAppSDKPackageDir)' == '' and '$(WindowsAppSDKSelfContained)' == '' and '$(OutputType)' != 'Library'">
  <WindowsAppSDKSelfContained>true</WindowsAppSDKSelfContained>
</PropertyGroup>
```

该条件把静态库误当作应用。导入的 SelfContained targets 将
`AddMicrosoftWindowsAppSDKPayloadFilesFromComponents` 加入
`GetCopyToOutputDirectoryItemsDependsOn`，收集 `runtimes-framework` 文件，
再以 `CopyToOutputDirectory=PreserveNewest` 沿 ProjectReference 传入 OpenNet。
旧的最终 `.build.appxrecipe` 确实包含这些文件，不只是目录中恰好残留了旧 DLL。

同样的条件同时存在于本机 Base `1.8.251216001` 和 `2.0.4`。
`4765ab5` 开始把 Toolkit 接入为源码 ProjectReference，是引入这条传播路径的历史节点；
这里没有把它宣称为经过逐提交运行二分证明的首次坏提交。

## 修复与验证方法

在 ModuleBuilder 导入 `Microsoft.Cpp.props` 后，将 `OutputType` 明确设置为
`Library`，使 WindowsAppSDK 按库处理。`ConfigurationType=StaticLibrary`、
`TargetExt=.lib`、`LibCompiled=true` 保持不变。

只设置 `WindowsAppSDKSelfContained=false` 不够：SDK 另一项相同的类型判断会要求
这个静态库添加 Runtime package reference。也不应把整个 OpenNet 改为 self-contained
来掩盖该传播问题。

验证重点是实际 payload，而非仅检查属性文本：

1. 对 ModuleBuilder 执行 `GetCopyToOutputDirectoryItems`，确认不再传出 framework 文件。
2. 对 OpenNet 执行同一 target，确认传递输出中没有 WindowsAppSDK `runtimes-framework`。
3. 检查最终 `.build.appxrecipe` 和实际 AppX 布局，再以 packaged identity 启动。
4. 记录运行模块全路径，确认 CoreMessagingXP 只剩 framework package 中的一个实例。

旧 AppX 目录必须随重新部署清理；仅重新编译 EXE 无法撤销以前复制进去的 DLL。
一次只隔离应用目录 CoreMessagingXP 的实验已越过 registrar 崩溃，但随后在仍为本地
副本的 Microsoft.UI.Input 中失败，说明只删除单个 DLL 不是完整工程修复。

启动代码、MainWindow、Sentry、AppLifecycle、XamlOptionalChanges、ThemeHelper、
composition engine 和包版本均未修改。

## 实测结果

- ModuleBuilder 的 Debug/Release × x64/ARM64 四种构建评估全部保持静态 `.lib`，
  `GetCopyToOutputDirectoryItems` 均不再传出文件。
- OpenNet x64 Debug 和 Release 的传递复制项不再含 framework payload。
- x64 Debug 应用重新构建成功，0 个错误；已有编译及 PRI qualifier 警告仍存在。
  构建使用现有 Toolkit 二进制（`BuildProjectReferences=false`），没有把整个解决方案
  或 ARM64 的编译成功包含在这项结论中。
- 新 Debug 最终 `.build.appxrecipe` 不再含 WindowsAppSDK `runtimes-framework`。
  按新清单同步已注册目录、隔离旧清单带入的 227 个 framework/metadata 文件后，
  新 EXE 的 **OpenNet 主窗口**正常显示并响应；主线程进入
  `FrameworkApplication::RunDesktopWindowMessageLoop → GetMessageW`。
- Release 保持原 EXE 和应用库不变，仅隔离同类 227 个旧部署文件，
  **OpenNet 使用向导**正常显示并响应；没有修改用户设置以强行跳过向导。
  这项结果是部署 A/B，不是完整 Release 重编译测试。
- 两种运行配置均核对了九个指定模块的完整路径：全部来自
  `Microsoft.WindowsAppRuntime.2_2.5.1.0_x64__8wekyb3d8bbwe`，
  CoreMessagingXP 只有一个加载实例。

本机原始证据保存在未提交的 `artifacts/registrar/`：
`live-capture.log`、`alpc-port-trace.log`、`hresult-d0000035.dmp`、
`modulebuilder-payload-before.json`、`modulebuilder-payload-after.json`、
`build-debug.log`、`debug-verified.json` 和 `release-verified.json`。
旧部署文件保存在同目录的 `debug-framework-backup` / `release-framework-backup`，
各自的 `*-payload-moves.json` 记录原路径及 SHA-256。
