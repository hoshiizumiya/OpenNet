#pragma once

#ifndef MVVM_FRAMEWORK_CORE_H
#define MVVM_FRAMEWORK_CORE_H

#include "Mvvm/Framework/Core/CanExecuteRequestedEventArgs.g.h"
#include "Mvvm/Framework/Core/CanExecuteCompletedEventArgs.g.h"
#include "Mvvm/Framework/Core/ExecuteRequestedEventArgs.g.h"
#include "Mvvm/Framework/Core/ExecuteCompletedEventArgs.g.h"
#include "Mvvm/Framework/Core/ExecutionStateChangedEventArgs.g.h"

#include "Mvvm/Framework/Core/ValidationRequestedEventArgs.g.h"
#include "Mvvm/Framework/Core/ValidationCompletedEventArgs.g.h"
#include "Mvvm/Framework/Core/ValidationErrorsChangedEventArgs.g.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::Mvvm::Framework::Core::implementation
{
    struct CanExecuteRequestedEventArgs : CanExecuteRequestedEventArgsT<CanExecuteRequestedEventArgs>
    {
        CanExecuteRequestedEventArgs(winrt::Windows::Foundation::IInspectable const& parameter)
            : m_parameter(parameter)
        {
        }

        winrt::Windows::Foundation::IInspectable Parameter() const { return m_parameter; }
        bool Handled() const { return m_handled; }
        void Handled(bool value) { m_handled = value; }

    private:
        winrt::Windows::Foundation::IInspectable m_parameter{ nullptr };
        bool m_handled{ false };
    };

    struct CanExecuteCompletedEventArgs : CanExecuteCompletedEventArgsT<CanExecuteCompletedEventArgs>
    {
        CanExecuteCompletedEventArgs(winrt::Windows::Foundation::IInspectable const& parameter, bool result)
            : m_parameter(parameter), m_result(result)
        {
        }

        winrt::Windows::Foundation::IInspectable Parameter() const { return m_parameter; }
        bool Result() const { return m_result; }

    private:
        winrt::Windows::Foundation::IInspectable m_parameter{ nullptr };
        bool m_result{ false };
    };

    struct ExecuteRequestedEventArgs : ExecuteRequestedEventArgsT<ExecuteRequestedEventArgs>
    {
        ExecuteRequestedEventArgs(winrt::Windows::Foundation::IInspectable const& parameter)
            : m_parameter(parameter)
        {
        }

        winrt::Windows::Foundation::IInspectable Parameter() const { return m_parameter; }

    private:
        winrt::Windows::Foundation::IInspectable m_parameter{ nullptr };
    };

    struct ExecuteCompletedEventArgs : ExecuteCompletedEventArgsT<ExecuteCompletedEventArgs>
    {
        ExecuteCompletedEventArgs(winrt::Windows::Foundation::IInspectable const& parameter, int32_t hresult)
            : m_parameter(parameter), m_error(hresult), m_succeeded(hresult >= 0)
        {
        }

        winrt::Windows::Foundation::IInspectable Parameter() const { return m_parameter; }
        bool Succeeded() const { return m_succeeded; }
        int32_t Error() const { return m_error; }

    private:
        winrt::Windows::Foundation::IInspectable m_parameter{ nullptr };
        int32_t m_error{ 0 };
        bool m_succeeded{ false };
    };

    struct ExecutionStateChangedEventArgs :
        ExecutionStateChangedEventArgsT<ExecutionStateChangedEventArgs>
    {
        ExecutionStateChangedEventArgs(
            winrt::Windows::Foundation::IInspectable const& parameter,
            Mvvm::Framework::Core::ExecutionState state,
            int32_t error) :
            m_parameter(parameter), m_state(state), m_error(error) {
        }

        winrt::Windows::Foundation::IInspectable Parameter() const { return m_parameter; }
        Mvvm::Framework::Core::ExecutionState State() const { return m_state; }
        int32_t Error() const { return m_error; }

    private:
        winrt::Windows::Foundation::IInspectable m_parameter{ nullptr };
        Mvvm::Framework::Core::ExecutionState m_state{};
        int32_t m_error{ 0 };
    };

    struct ValidationRequestedEventArgs : ValidationRequestedEventArgsT<ValidationRequestedEventArgs>
    {
        ValidationRequestedEventArgs(winrt::hstring const& propertyName,
            winrt::Windows::Foundation::IInspectable const& newValue)
            : m_propertyName(propertyName), m_newValue(newValue) {
        }

        winrt::hstring PropertyName() const { return m_propertyName; }
        winrt::Windows::Foundation::IInspectable NewValue() const { return m_newValue; }

        bool Handled() const { return m_handled; }
        void Handled(bool value) { m_handled = value; }

        bool Cancel() const { return m_cancel; }
        void Cancel(bool value) { m_cancel = value; }

    private:
        winrt::hstring m_propertyName{};
        winrt::Windows::Foundation::IInspectable m_newValue{ nullptr };
        bool m_handled{ false };
        bool m_cancel{ false };
    };

    struct ValidationCompletedEventArgs : ValidationCompletedEventArgsT<ValidationCompletedEventArgs>
    {
        ValidationCompletedEventArgs(
            winrt::hstring const& propertyName,
            winrt::Windows::Foundation::IInspectable const& newValue,
            bool isValid,
            winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> const& errors)
            : m_propertyName(propertyName),
            m_newValue(newValue),
            m_isValid(isValid),
            m_errors(errors) {
        }

        winrt::hstring PropertyName() const { return m_propertyName; }
        winrt::Windows::Foundation::IInspectable NewValue() const { return m_newValue; }
        bool IsValid() const { return m_isValid; }
        winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> Errors() const { return m_errors; }

    private:
        winrt::hstring m_propertyName{};
        winrt::Windows::Foundation::IInspectable m_newValue{ nullptr };
        bool m_isValid{ false };
        winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> m_errors{ nullptr };
    };

    struct ValidationErrorsChangedEventArgs : ValidationErrorsChangedEventArgsT<ValidationErrorsChangedEventArgs>
    {
        ValidationErrorsChangedEventArgs(
            winrt::hstring const& propertyName,
            winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> const& errors)
            : m_propertyName(propertyName), m_errors(errors) {
        }

        winrt::hstring PropertyName() const { return m_propertyName; }
        winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> Errors() const { return m_errors; }

    private:
        winrt::hstring m_propertyName{};
        winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> m_errors{ nullptr };
    };
}

namespace winrt::Mvvm::Framework::Core::factory_implementation
{
    struct CanExecuteRequestedEventArgs
        : CanExecuteRequestedEventArgsT<CanExecuteRequestedEventArgs, 
        implementation::CanExecuteRequestedEventArgs>
    {
    };

    struct CanExecuteCompletedEventArgs
        : CanExecuteCompletedEventArgsT<CanExecuteCompletedEventArgs, 
        implementation::CanExecuteCompletedEventArgs>
    {
    };

    struct ExecuteRequestedEventArgs
        : ExecuteRequestedEventArgsT<ExecuteRequestedEventArgs, 
        implementation::ExecuteRequestedEventArgs>
    {
    };

    struct ExecuteCompletedEventArgs
        : ExecuteCompletedEventArgsT<ExecuteCompletedEventArgs, 
        implementation::ExecuteCompletedEventArgs>
    {
    };

    struct ExecutionStateChangedEventArgs
        : ExecutionStateChangedEventArgsT<ExecutionStateChangedEventArgs,
        implementation::ExecutionStateChangedEventArgs>
    {
    };

    struct ValidationRequestedEventArgs
        : ValidationRequestedEventArgsT<ValidationRequestedEventArgs, 
        implementation::ValidationRequestedEventArgs>
    {
    };

    struct ValidationCompletedEventArgs
        : ValidationCompletedEventArgsT<ValidationCompletedEventArgs, 
        implementation::ValidationCompletedEventArgs>
    {
    };

    struct ValidationErrorsChangedEventArgs
        : ValidationErrorsChangedEventArgsT<ValidationErrorsChangedEventArgs,
        implementation::ValidationErrorsChangedEventArgs>
    {
    };
}

#endif // MVVM_FRAMEWORK_CORE_H