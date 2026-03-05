# Copilot Instructions

## General Guidelines
- First general instruction
- Second general instruction
- When updating documentation, explicitly ensure the user can see the actual edits in the target markdown file (avoid claiming changes without applying them).

## Code Style
- Use specific formatting rules
- Follow naming conventions

## Project-Specific Rules
- TitleCard is implemented as a custom Control with its template defined in a separate resource dictionary file named `TitleCard_ResourceDictionary.xaml` (not a `TitleCard.xaml` control markup file).
- For C++ WinRT, the IDL namespace `OpenNet.UI.Xaml.Behaviors` maps to the generated file path `UI/Xaml/Behaviors/XXX.g.h`. The generated `.g.h` files are placed according to the namespace structure in the Generated Files directory, regardless of the subdirectory of the `.idl` file. In the corresponding `.h` files, include using `#include "UI/Xaml/Behaviors/XXX.g.h"` instead of relative paths.

## CppWinRT tips
- C++/WinRT 协程线程切换指南，适用于 C++/WinRT 与 UI 框架（WinUI 3、UWP）
  - 1. 基本原则 
Windows UI 框架遵循：UI 控件只能在 UI 线程访问，耗时操作必须在后台线程执行，协程代码通常遵循如下流程：
```
UI thread
    ↓
resume_background()
    ↓
后台执行耗时任务
    ↓
resume_foreground(dispatcher)
    ↓
更新 UI
```
2. 推荐线程切换 API：切换到后台线程 `co_await winrt::resume_background();`. 用途：CPU 密集型计算/IO 操作/网络请求/文件处理

调度到 Windows threadpool不阻塞 UI 线程

winrt2.0 的切换回 UI 线程：`co_await winrt::resume_foreground(dispatcher);`

示例：

co_await winrt::resume_foreground(textBlock.DispatcherQueue());

特点：

将 continuation post 到 UI dispatcher

不占用 threadpool 线程，但可能需要 UI 线程安全直到恢复

3. 不推荐使用的模式
winrt::apartment_context

示例：

winrt::apartment_context ui;

co_await winrt::resume_background();

...

co_await ui;

问题：

实现会占用一个 threadpool 线程等待恢复

大量协程可能导致 threadpool exhaustion

线程来源隐式（不明确是否 UI 线程）

已被更明确的 API 取代

历史说明：

早期版本还可能 阻塞 STA 线程，后来已在 C++/WinRT 中修复，但仍存在线程池资源浪费问题。

因此：

避免使用 apartment_context 作为 UI 恢复机制
4. 更安全的 UI 恢复

在某些情况下，resume_foreground 可能遇到 dispatcher 已关闭的问题。

更安全的替代：

wil::resume_foreground(dispatcher)

来自 Windows Implementation Library

优点：

处理 dispatcher shutdown

更健壮的错误处理

5. 推荐协程结构模板

标准模式：

IAsyncAction Example()
{
    // UI thread

    co_await winrt::resume_background();

    // 后台线程
    auto result = DoHeavyWork();

    co_await winrt::resume_foreground(dispatcher);

    // UI thread
    UpdateUI(result);
}
6. 规则
页面构造函数中不应该执行和 UI 控件等任何不必要的操作，除非是 Composition 相关的内容要操作界面了，可以使用 event_tocken 等异步或切换让事件不阻塞。然后优先co_await winrt::resume_background();co_await winrt::resume_foreground(dispatcher);不应生成winrt::apartment_contextco_await apartment_context除非明确需要恢复 原始 COM apartment。