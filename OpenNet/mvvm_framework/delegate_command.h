//*********************************************************
//
//    Copyright (c) Millennium R&D Team. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
//
//  File Name:    delegate_command.h
//  Description:  Provide a class template for implementing delegate commands.
//                This class template implements the 
//                Microsoft.UI.Xaml.Input.ICommand interface with 
//                `CanExecuteChanged`, `Execute`, and `CanExecute`
//                methods, and provides mechanisms for registering dependency 
//                properties and automatically handling execution conditions.
//  Created:      2025-07-31
//  Author:       AlexAlva(LianYou)
//
//  Modified:     2025-08-02  AlexAlva(LianYou)  [BugFix] Fixed a bug where the notification event handler for registered properties was not properly removed, 
//                                               using weak references to resolve circular reference issues.
//                2025-08-01  AlexAlva(LianYou)  Add notification dependency command and auto-execute command
//
//*********************************************************
#pragma once
#ifndef __MVVM_CPPWINRT_DELEGATE_COMMAND_H_INCLUDED
#define __MVVM_CPPWINRT_DELEGATE_COMMAND_H_INCLUDED

#include <functional>
#include <type_traits>
//#include <debugapi.h>
#include <mvvm_framework/mvvm_diagnostics.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>

namespace mvvm
{
    using namespace mvvm::diagnostics;
    using namespace mvvm::exceptions;

    using RelayDependencyCondition = std::function<bool(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const&)>;
    using AutoExecuteCondition = std::function<bool(winrt::Windows::Foundation::IInspectable const&)>;

    struct DependencyRegistration
    {
        winrt::hstring                  propertyName;
        RelayDependencyCondition        relayDependencyCondition;       // Optional
        AutoExecuteCondition            autoExecuteCondition;           // Optional
    };

    struct CanExecuteRequestedEventArgs : winrt::Windows::Foundation::IInspectable
    {
        CanExecuteRequestedEventArgs()
        {
            m_handled = CreateBooleanInspectable(false);
        }

        bool Handled() const
        {
            if (auto val = m_handled.try_as<winrt::Windows::Foundation::IPropertyValue>())
            {
                return val.GetBoolean();
            }
            return false;
        }

        void Handled(bool value)
        {
            m_handled = CreateBooleanInspectable(value);
        }

    protected:
        static winrt::Windows::Foundation::IInspectable CreateBooleanInspectable(bool value)
        {
            auto propertyValueFactory = winrt::get_activation_factory<winrt::Windows::Foundation::PropertyValue,
                winrt::Windows::Foundation::IPropertyValueStatics>();
            return propertyValueFactory.CreateBoolean(value);
        }

    private:
        winrt::Windows::Foundation::IInspectable Parameter;
        winrt::Windows::Foundation::IInspectable m_handled;
    };


    template <typename Parameter>
    struct delegate_command
        : winrt::implements<delegate_command<Parameter>, winrt::Microsoft::UI::Xaml::Input::ICommand>
    {
    #pragma region constructors

        delegate_command() = default;

        template <typename ExecuteHandler>
        delegate_command(ExecuteHandler&& executeHandler)
            : m_executeHandler(std::move(executeHandler))
        {
            static_assert(std::is_invocable_v<ExecuteHandler, Parameter>);
        }

        template <typename ExecuteHandler, typename CanExecuteHandler>
        delegate_command(ExecuteHandler&& executeHandler, CanExecuteHandler&& canExecuteHandler)
            : m_executeHandler(std::move(executeHandler))
            , m_canExecuteHandler(std::move(canExecuteHandler))
        {
            static_assert(std::is_invocable_v<ExecuteHandler, Parameter>);
            static_assert(std::is_invocable_r_v<bool, CanExecuteHandler, Parameter>);
        }

        ~delegate_command()
        {
            // remove RelayDependency
            for (size_t i = 0; i < m_dependencyNotifiers.size(); ++i)
            {
                if (auto inpc = m_dependencyNotifiers[i].get())
                {
                    inpc.PropertyChanged(m_dependencyTokens[i]);
                }
            }

            // remove AutoExecute
            for (size_t i = 0; i < m_autoExecuteNotifiers.size(); ++i)
            {
                if (auto inpc = m_autoExecuteNotifiers[i].get())
                {
                    inpc.PropertyChanged(m_autoExecuteTokens[i]);
                }
            }
        }

        template <typename ExecuteHandler>
        void Initialize(ExecuteHandler&& executeHandler)
        {
            static_assert(std::is_invocable_v<ExecuteHandler, Parameter>);

            if (m_executeHandler)
            {
                throw winrt::hresult_changed_state(L"Object has already been initialized: winrt::xaml::delegate_command");
            }

            m_executeHandler = std::forward<ExecuteHandler>(executeHandler);
        }

        template <typename ExecuteHandler, typename CanExecuteHandler>
        void Initialize(ExecuteHandler&& executeHandler, CanExecuteHandler&& canExecuteHandler)
        {
            static_assert(std::is_invocable_v<ExecuteHandler, Parameter>);
            static_assert(std::is_invocable_r_v<bool, CanExecuteHandler, Parameter>);

            Initialize(std::forward<ExecuteHandler>(executeHandler));
            m_canExecuteHandler = std::forward<CanExecuteHandler>(canExecuteHandler);
        }

        /*template <typename ExecuteHandler>
        void Initialize(winrt::Windows::Foundation::IInspectable const& ownerObject, ExecuteHandler&& executeHandler)
        {
            if (ownerObject)
            {
                m_ownerObject = winrt::make_weak(ownerObject);
            }
            Initialize(std::forward<ExecuteHandler>(executeHandler));
        }

        template <typename ExecuteHandler, typename CanExecuteHandler>
        void Initialize(winrt::Windows::Foundation::IInspectable const& ownerObject, ExecuteHandler&& executeHandler, CanExecuteHandler&& canExecuteHandler)
        {
            if (ownerObject)
            {
                m_ownerObject = winrt::make_weak(ownerObject);
            }
            Initialize(std::forward<ExecuteHandler>(executeHandler), std::forward<CanExecuteHandler>(canExecuteHandler));
        }*/

    #pragma endregion

    #pragma region ICommand

        winrt::event_token CanExecuteChanged(winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler)
        {
            return m_eventCanExecuteChanged.add(handler);
        }

        void CanExecuteChanged(winrt::event_token token)
        {
            m_eventCanExecuteChanged.remove(token);
        }

        // ICommand required methods
        bool CanExecute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            //auto strongReference = m_ownerObject.get();
            //if (m_ownerObject && !strongReference)
            //{
            //    return false;
            //}

            return CanExecutePrivate(parameter);
        }

        void Execute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            //auto strongReference = m_ownerObject.get();
            //if (m_ownerObject && !strongReference)
            //{
            //    return;
            //}

            if (this->CanExecutePrivate(parameter))
            {
                if constexpr (std::is_same_v<Parameter, winrt::Windows::Foundation::IInspectable>)
                {
                    std::invoke(m_executeHandler, parameter);
                }
                else if constexpr (std::is_convertible_v<Parameter, winrt::Windows::Foundation::IInspectable>)
                {
                    std::invoke(m_executeHandler, parameter.try_as<Parameter>());
                }
                else
                {
                    std::invoke(m_executeHandler, winrt::unbox_value(parameter));
                }
            }
        }

    #pragma endregion

    #pragma region extensions

        // Adds a dependency to the command, which will trigger CanExecuteChanged when the dependency changes.
        void OnAttachPropertyChanged(
            winrt::hstring const& prop,
            RelayDependencyCondition const& cond,
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
        {
            if (prop.empty() || args.PropertyName() == prop)
            {
                if (!cond || cond(sender, args))
                {
                    raise_CanExecuteChanged();
                }
            }
        }

        void AttachProperty(
            winrt::Windows::Foundation::IInspectable const& notifier,
            winrt::hstring const& propertyName,
            RelayDependencyCondition condition = nullptr)
        {
            if (auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>())
            {
                auto weakThis = this->get_weak();
                auto token = inpc.PropertyChanged(
                    [weakThis, prop = propertyName, cond = std::move(condition)](
                        auto&& sender, auto&& args)
                    {
                        if (auto self = weakThis.get())
                        {
                            self->OnAttachPropertyChanged(prop, cond, sender, args);
                        }
                    });

                m_dependencyTokens.push_back(token);
                m_dependencyNotifiers.push_back(winrt::make_weak(inpc));
            }
        }

        void OnAutoExecuteCondChanged(
            AutoExecuteCondition const& cond,
            winrt::Windows::Foundation::IInspectable const& sender)
        {
            if (cond && cond(sender))
            {
                Execute(winrt::Windows::Foundation::IInspectable{ nullptr });
            }
        }

        // Adds an auto-execute dependency to the command, which will trigger Execute when the dependency changes.
        void RegisterAutoExecute(
            winrt::Windows::Foundation::IInspectable const& notifier,
            AutoExecuteCondition condition)
        {
            if (auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>())
            {
                auto weakThis = this->get_weak();
                auto token = inpc.PropertyChanged(
                    [weakThis, cond = std::move(condition)](
                        auto&& sender, auto const&)
                    {
                        if (auto self = weakThis.get())
                        {
                            self->OnAutoExecuteCondChanged(cond, sender);
                        }
                    });

                m_autoExecuteTokens.push_back(token);
                m_autoExecuteNotifiers.push_back(winrt::make_weak(inpc));
            }
        }

        // Create a delegate_command that contains notification handlers and rechecks the command status depending on a change in one of multiple dependent properties.
        template <typename ExecuteHandler, typename CanExecuteHandler>
        delegate_command(
            winrt::Windows::Foundation::IInspectable const& notifier,
            ExecuteHandler&& executeHandler,
            CanExecuteHandler&& canExecuteHandler,
            std::vector<winrt::hstring> dependencyProps = {})
            : m_executeHandler(std::move(executeHandler)),
            m_canExecuteHandler(std::move(canExecuteHandler))
        {
            if (!notifier)
                throw winrt::hresult_invalid_argument(L"Notifier cannot be null.");

            auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>();
            if (!inpc) return;

            // Add dependency property
            for (auto const& prop : dependencyProps)
            {
                AttachProperty(notifier, prop);
            }
        }

        template <typename ExecuteHandler, typename CanExecuteHandler>
        delegate_command(
            winrt::Windows::Foundation::IInspectable const& notifier,
            ExecuteHandler&& executeHandler,
            CanExecuteHandler&& canExecuteHandler,
            std::vector<DependencyRegistration> dependencies)
            : m_executeHandler(std::move(executeHandler)),
            m_canExecuteHandler(std::move(canExecuteHandler))
        {
            static_assert(std::is_invocable_v<ExecuteHandler, Parameter>);
            static_assert(std::is_invocable_r_v<bool, CanExecuteHandler, Parameter>);

            if (!notifier)
                MVVM_THROW(invalid_object, L"Invalid parameter `Notifier` is null.");

            auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>();
            if (!inpc) return;
            
            for (size_t i = 0; i < dependencies.size(); ++i)
            {
                auto const& dep = dependencies[i];
                bool emptyName = dep.propertyName.empty();
                bool nullRelay = !dep.relayDependencyCondition;
                bool nullAuto = !dep.autoExecuteCondition;

                if (emptyName && nullRelay && nullAuto)
                {
                    std::wstring message = L"\ndelegate_command ctor failed: DependencyRegistration at index "
                        + std::to_wstring(i)
                        + L" is invalid: propertyName empty, relayDependencyCondition null, autoExecuteCondition null.\n";
                    throw winrt::hresult_invalid_argument(message);
                }
            #ifdef _DEBUG
                else if (emptyName)
                {
                    std::wstring message = L"\ndelegate_command ctor warning: DependencyRegistration at index "
                        + std::to_wstring(i)
                        + L" has empty propertyName.\n";

                    OutputDebugStringW(message.c_str());
                    if (IsDebuggerPresent())
                    {
                        __debugbreak();
                    }
                }
            #endif

                AttachProperty(notifier, dep.propertyName, dep.relayDependencyCondition);

                if (dep.autoExecuteCondition)
                {
                    RegisterAutoExecute(notifier,
                        [cond = dep.autoExecuteCondition](auto const& sender)
                        {
                            return cond(sender);
                        });
                }
            }
        }
    #pragma endregion

    #pragma region methods

        operator bool() { return m_executeHandler; }

        void raise_CanExecuteChanged()
        {
            if (m_eventCanExecuteChanged)
            {
                m_eventCanExecuteChanged(*this, winrt::Windows::Foundation::IInspectable{ nullptr });
            }
        }

    #pragma endregion

    #pragma region private implementation

    private:
        bool CanExecutePrivate(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            if (!m_canExecuteHandler)
            {
                return true;
            }
            else if constexpr (std::is_same_v<Parameter, winrt::Windows::Foundation::IInspectable>)
            {
                return std::invoke(m_canExecuteHandler, parameter);
            }
            else if constexpr (std::is_convertible_v<Parameter, winrt::Windows::Foundation::IInspectable>)
            {
                return std::invoke(m_canExecuteHandler, parameter.try_as<Parameter>());
            }
            else
            {
                return std::invoke(m_canExecuteHandler, winrt::unbox_value(parameter));
            }
        }

    #pragma endregion

    #pragma region instance fields
    private:
        //winrt::weak_ref<winrt::Windows::Foundation::IInspectable> m_ownerObject;

        std::function<void(Parameter const&)> m_executeHandler;
        std::function<bool(Parameter const&)> m_canExecuteHandler;
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> m_eventCanExecuteChanged;
        
        std::vector<winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>> m_dependencyNotifiers;
        std::vector<winrt::event_token> m_dependencyTokens;
        std::vector<winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>> m_autoExecuteNotifiers;
        std::vector<winrt::event_token> m_autoExecuteTokens;
    #pragma endregion
    };
}

#endif // __MVVM_CPPWINRT_DELEGATE_COMMAND_H_INCLUDED
