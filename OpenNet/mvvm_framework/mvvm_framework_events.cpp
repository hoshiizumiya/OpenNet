#include "pch.h"
#include "mvvm_framework_events.h"

#if __has_include("Mvvm/Framework/Core/CanExecuteRequestedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/CanExecuteRequestedEventArgs.g.cpp"
#endif
#if __has_include("Mvvm/Framework/Core/CanExecuteCompletedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/CanExecuteCompletedEventArgs.g.cpp"
#endif
#if __has_include("Mvvm/Framework/Core/ExecuteRequestedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/ExecuteRequestedEventArgs.g.cpp"
#endif
#if __has_include("Mvvm/Framework/Core/ExecuteCompletedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/ExecuteCompletedEventArgs.g.cpp"
#endif
#if __has_include("Mvvm/Framework/Core/ExecutionStateChangedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/ExecutionStateChangedEventArgs.g.cpp"
#endif

#if __has_include("Mvvm/Framework/Core/ValidationRequestedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/ValidationRequestedEventArgs.g.cpp"
#endif
#if __has_include("Mvvm/Framework/Core/ValidationCompletedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/ValidationCompletedEventArgs.g.cpp"
#endif
#if __has_include("Mvvm/Framework/Core/ValidationErrorsChangedEventArgs.g.cpp")
#include "Mvvm/Framework/Core/ValidationErrorsChangedEventArgs.g.cpp"
#endif

namespace winrt::Mvvm::Framework::Core::implementation
{
    // 模板基类已在头文件中实现，这里不需要额外方法实现
}