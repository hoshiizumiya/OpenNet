#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>


// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Globalization.NumberFormatting.h>
#include <winrt/Windows.ApplicationModel.Activation.h>  // Four common kinds of application activation
#include <winrt/Windows.UI.Xaml.Interop.h> // For using xaml_typename
#include <winrt/Microsoft.UI.Windowing.h>  // For using AppTitleBar
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h> // Timeline and KeyFrame animations
#include <winrt/Microsoft.UI.Xaml.Controls.AnimatedVisuals.h> // Pre-made icon animation
#include <winrt/Microsoft.UI.Xaml.Documents.h>  // Rich text content and inline formatting
// Provides simplified access to app resources, such as strings, that are defined using basic naming conventions.
// In releases before Windows App SDK 1.0 Preview 1, this namespace was called Microsoft.ApplicationModel.Resources.
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <winrt/Microsoft.UI.Input.h>

// Windows 运行时服务
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Data.Json.h>

#include <wil/cppwinrt_helpers.h>
// Cpp 常用库头文件
// 基本
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>
#include <bit>

// 字符串/文本
#include <string>
#include <string_view>
#include <charconv>     // 数字<->文本高速转换
#include <format>       // C++20 格式化

// 容器与算法
#include <vector>
#include <array>
#include <deque>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <span>         // 视图类型

// 资源/内存
#include <memory>
#include <new>

// 时间/并发
#include <chrono>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <stop_token>   // C++20 可取消
#include <semaphore>    // C++20

// 工具
#include <functional>
#include <optional>
#include <variant>
#include <tuple>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <system_error>
