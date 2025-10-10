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
#include <memory> // std::destroy_at, std::construct_at
//#include <debugapi.h>

#include <mvvm_framework/mvvm_diagnostics.h>
#include <mvvm_framework/mvvm_framework_events.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>

namespace mvvm
{
    using namespace mvvm::diagnostics;
    using namespace mvvm::exceptions;

    using RelayDependencyCondition = std::function<bool(winrt::Windows::Foundation::IInspectable const&, 
        winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const&)>;
    using AutoExecuteCondition = std::function<bool(winrt::Windows::Foundation::IInspectable const&)>;

    struct DependencyRegistration
    {
        winrt::hstring                  propertyName;
        RelayDependencyCondition        relayDependencyCondition;       // Optional
        AutoExecuteCondition            autoExecuteCondition;           // Optional
    };

    template <typename Parameter>
    struct DelegateCommand
        : winrt::implements<DelegateCommand<Parameter>,
        winrt::Microsoft::UI::Xaml::Input::ICommand,
        winrt::Mvvm::Framework::Core::ICommandCleanup>
    {
        using NakedParameterType = std::conditional_t<std::is_same_v<Parameter, void>, void,
            std::remove_const_t<std::remove_reference_t<Parameter>>>;
        using ConstParameterType = std::conditional_t<std::is_same_v<Parameter, void>, void,
            std::add_const_t<NakedParameterType>>;
        using ExecuteHandler = std::function<void(std::add_lvalue_reference_t<ConstParameterType>)>;
        using CanExecuteHandler = std::function<bool(std::add_lvalue_reference_t<ConstParameterType>)>;

    #pragma region constructors

        DelegateCommand() = default;
        DelegateCommand(std::nullptr_t) noexcept {}

        DelegateCommand(ExecuteHandler&& executeHandler)
            : m_executeHandler(std::move(executeHandler)) {
        }

        DelegateCommand(ExecuteHandler&& executeHandler, CanExecuteHandler&& canExecuteHandler)
            : m_executeHandler(std::move(executeHandler)),
            m_canExecuteHandler(std::move(canExecuteHandler)) {
        }

        template <typename ExecuteHandlerT, typename CanExecuteHandlerT>
        DelegateCommand(
            winrt::Windows::Foundation::IInspectable const& notifier,
            ExecuteHandlerT&& executeHandler,
            CanExecuteHandlerT&& canExecuteHandler,
            std::vector<DependencyRegistration> const& dependencies)
            : m_executeHandler(std::move(executeHandler)),
            m_canExecuteHandler(std::move(canExecuteHandler))
        {
            AttachDependencies(notifier, dependencies);
        }

        ~DelegateCommand()
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

    #pragma endregion

    #pragma region ICommand

        winrt::event_token CanExecuteChanged(
            winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler)
        {
            return m_eventCanExecuteChanged.add(handler);
        }

        void CanExecuteChanged(winrt::event_token token)
        {
            m_eventCanExecuteChanged.remove(token);
        }

        // CanExecuteRequested
        winrt::event_token CanExecuteRequested(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, 
            winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs> const& handler)
        {
            return m_eventCanExecuteRequested.add(handler);
        }
        void CanExecuteRequested(winrt::event_token const& token) noexcept
        {
            m_eventCanExecuteRequested.remove(token);
        }

        // CanExecuteCompleted
        winrt::event_token CanExecuteCompleted(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs> const& handler)
        {
            return m_eventCanExecuteCompleted.add(handler);
        }
        void CanExecuteCompleted(winrt::event_token const& token) noexcept
        {
            m_eventCanExecuteCompleted.remove(token);
        }

        // ExecuteRequested
        winrt::event_token ExecuteRequested(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, 
            winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs> const& handler)
        {
            return m_eventExecuteRequested.add(handler);
        }
        void ExecuteRequested(winrt::event_token const& token) noexcept
        {
            m_eventExecuteRequested.remove(token);
        }

        // ExecuteCompleted
        winrt::event_token ExecuteCompleted(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, 
            winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs> const& handler)
        {
            return m_eventExecuteCompleted.add(handler);
        }
        void ExecuteCompleted(winrt::event_token const& token) noexcept
        {
            m_eventExecuteCompleted.remove(token);
        }

        // ICommand required methods
        bool CanExecute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs reqArgs(parameter);
            if (m_eventCanExecuteRequested) m_eventCanExecuteRequested(*this, reqArgs);

            bool state = false;
            if (!reqArgs.Handled())
            {
                // 没有执行处理器，此时默认认为不可执行
                if (!m_executeHandler)
                {
                    state = false;
                }
                else if (!m_canExecuteHandler)
                {
                    // 没有 CanExecute 处理器但有 Execute，此时默认认为可执行
                    state = true;
                }
                else
                {
                    if constexpr (std::is_same_v<Parameter, void>)
                        state = std::invoke(m_canExecuteHandler);
                    else if constexpr (std::is_same_v<NakedParameterType, winrt::Windows::Foundation::IInspectable>)
                        state = std::invoke(m_canExecuteHandler, parameter);
                    else if constexpr (std::is_convertible_v<NakedParameterType, winrt::Windows::Foundation::IInspectable>)
                        state = std::invoke(m_canExecuteHandler, parameter.try_as<NakedParameterType>());
                    else
                        state = std::invoke(m_canExecuteHandler, winrt::unbox_value_or<NakedParameterType>(parameter, {}));
                }
            }

            if (m_eventCanExecuteCompleted)
                m_eventCanExecuteCompleted(*this, winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs(parameter, state));

            return state;
        }

        void Execute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            if (m_eventExecuteRequested)
                m_eventExecuteRequested(*this, winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs(parameter));

            winrt::hresult error = S_OK;
            try
            {
                if constexpr (std::is_same_v<Parameter, void>)
                    std::invoke(m_executeHandler);
                else if constexpr (std::is_same_v<NakedParameterType, winrt::Windows::Foundation::IInspectable>)
                    std::invoke(m_executeHandler, parameter);
                else if constexpr (std::is_convertible_v<NakedParameterType, winrt::Windows::Foundation::IInspectable>)
                    std::invoke(m_executeHandler, parameter.try_as<NakedParameterType>());
                else
                    std::invoke(m_executeHandler, winrt::unbox_value_or<NakedParameterType>(parameter, {}));
            }
            catch (winrt::hresult_error const& e) { error = e.code(); }
            catch (...) { error = E_FAIL; }

            if (m_eventExecuteCompleted)
                m_eventExecuteCompleted(*this, winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs(parameter, error));
        }

        void RaiseCanExecuteChangedEvent()
        {
            if (m_eventCanExecuteChanged)
                m_eventCanExecuteChanged(*this, nullptr);
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
                    RaiseCanExecuteChangedEvent();
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
        void RegisterAutoExecuteCond(
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

        void AttachDependencies(
            winrt::Windows::Foundation::IInspectable const& notifier,
            std::vector<DependencyRegistration> const& dependencies)
        {
            if (auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>())
            {
                for (auto const& dep : dependencies)
                {
                    AttachProperty(notifier, dep.propertyName, dep.relayDependencyCondition);
                    if (dep.autoExecuteCondition)
                        RegisterAutoExecuteCond(notifier, dep.autoExecuteCondition);
                }
            }
        }

        // TODO:  i need to make it clear that the notifier must be a DependencyObject,
        // and that the property name must be a valid dependency property name.
        // 
        // use vector<hsring> as a parameter is not a good idea, because it's hard to maintain the order of the dependencies.
        // 
        // Create a DelegateCommand that contains notification handlers and rechecks the command status depending on a change in one of multiple dependent properties.
        //template <typename ExecuteHandler, typename CanExecuteHandler>
        //DelegateCommand(
        //    winrt::Windows::Foundation::IInspectable const& notifier,
        //    ExecuteHandler&& executeHandler,
        //    CanExecuteHandler&& canExecuteHandler,
        //    std::vector<winrt::hstring> dependencyProps = {})
        //    : m_executeHandler(std::move(executeHandler)),
        //    m_canExecuteHandler(std::move(canExecuteHandler))
        //{
        //    if (!notifier)
        //        throw winrt::hresult_invalid_argument(L"Notifier cannot be null.");

        //    auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>();
        //    if (!inpc) return;

        //    // Add dependency property
        //    for (auto const& prop : dependencyProps)
        //    {
        //        AttachProperty(notifier, prop);
        //    }
        //}

        // i had changed this without testing it(without parameter check in new ctor above this one).
        // i think it's better to use a struct to store the dependency registration information.
       /* template <typename ExecuteHandler, typename CanExecuteHandler>
        DelegateCommand(
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
                    RegisterAutoExecuteCond(notifier,
                        [cond = dep.autoExecuteCondition](auto const& sender)
                        {
                            return cond(sender);
                        });
                }
            }
        }*/

        // ======= 取消注册（Detach/Unregister） =======

        // 仅移除“CanExecute 重新评估”的属性依赖（RelayDependency）
        void DetachRelayDependencies() noexcept
        {
            for (size_t i = 0; i < m_dependencyNotifiers.size(); ++i)
                if (auto inpc = m_dependencyNotifiers[i].get())
                    inpc.PropertyChanged(m_dependencyTokens[i]);

            m_dependencyNotifiers.clear();
            m_dependencyTokens.clear();
        }

        // 仅移除“自动执行”的属性依赖（AutoExecute）
        void DetachAutoExecuteDependencies() noexcept
        {
            for (size_t i = 0; i < m_autoExecuteNotifiers.size(); ++i)
                if (auto inpc = m_autoExecuteNotifiers[i].get())
                    inpc.PropertyChanged(m_autoExecuteTokens[i]);

            m_autoExecuteNotifiers.clear();
            m_autoExecuteTokens.clear();
        }

        // 移除当前命令上所有的依赖
        void DetachAllDependencies() noexcept
        {
            DetachRelayDependencies();
            DetachAutoExecuteDependencies();
        }

        // 清理已过期（notifier 已销毁）的订阅，返回移除的数量
        size_t PruneExpiredDependencies() noexcept
        {
            size_t pruned = 0;

            // RelayDependency
            for (size_t i = m_dependencyNotifiers.size(); i-- > 0; )
            {
                if (!m_dependencyNotifiers[i].get())
                {
                    m_dependencyNotifiers.erase(m_dependencyNotifiers.begin() + i);
                    m_dependencyTokens.erase(m_dependencyTokens.begin() + i);
                    ++pruned;
                }
            }

            // AutoExecute
            for (size_t i = m_autoExecuteNotifiers.size(); i-- > 0; )
            {
                if (!m_autoExecuteNotifiers[i].get())
                {
                    m_autoExecuteNotifiers.erase(m_autoExecuteNotifiers.begin() + i);
                    m_autoExecuteTokens.erase(m_autoExecuteTokens.begin() + i);
                    ++pruned;
                }
            }

            return pruned;
        }

        // 取消附加来自特定 INotifyPropertyChanged 的依赖（RelayDependency/AutoExecute）
        void DetachFrom(winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged const& notifier) noexcept
        {
            auto const id = winrt::get_abi(notifier);

            // RelayDependency
            for (size_t i = m_dependencyNotifiers.size(); i-- > 0; )
            {
                if (auto strong = m_dependencyNotifiers[i].get())
                {
                    if (winrt::get_abi(strong) == id)
                    {
                        strong.PropertyChanged(m_dependencyTokens[i]);
                        m_dependencyNotifiers.erase(m_dependencyNotifiers.begin() + i);
                        m_dependencyTokens.erase(m_dependencyTokens.begin() + i);
                    }
                }
                else
                {
                    // 清理已过期的订阅
                    m_dependencyNotifiers.erase(m_dependencyNotifiers.begin() + i);
                    m_dependencyTokens.erase(m_dependencyTokens.begin() + i);
                }
            }

            // AutoExecute
            for (size_t i = m_autoExecuteNotifiers.size(); i-- > 0; )
            {
                if (auto strong = m_autoExecuteNotifiers[i].get())
                {
                    if (winrt::get_abi(strong) == id)
                    {
                        strong.PropertyChanged(m_autoExecuteTokens[i]);
                        m_autoExecuteNotifiers.erase(m_autoExecuteNotifiers.begin() + i);
                        m_autoExecuteTokens.erase(m_autoExecuteTokens.begin() + i);
                    }
                }
                else
                {
                    m_autoExecuteNotifiers.erase(m_autoExecuteNotifiers.begin() + i);
                    m_autoExecuteTokens.erase(m_autoExecuteTokens.begin() + i);
                }
            }
        }

        // ======= 重置处理器 / 清空订阅者 =======

        // 清空 Execute/CanExecute 的委托
        void ResetHandlers() noexcept
        {
            m_executeHandler = {};
            m_canExecuteHandler = {};
        }

        // 清空命令外部订阅事件的订阅者（CanExecuteChanged/Requested/...）
        void ClearAllSubscribers() noexcept
        {
            ResetEventInPlace(m_eventCanExecuteChanged);
            ResetEventInPlace(m_eventCanExecuteRequested);
            ResetEventInPlace(m_eventCanExecuteCompleted);
            ResetEventInPlace(m_eventExecuteRequested);
            ResetEventInPlace(m_eventExecuteCompleted);
        }

        void Cancel() noexcept
        {
            /* 同步命令中不需要在这里做任何事 */
            ResetEventInPlace(m_eventCanExecuteChanged);
            ResetEventInPlace(m_eventCanExecuteRequested);
            ResetEventInPlace(m_eventCanExecuteCompleted);
            ResetEventInPlace(m_eventExecuteRequested);
            ResetEventInPlace(m_eventExecuteCompleted);
        }

        // 判断是否有依赖（RelayDependency/AutoExecute）
        bool HasDependencies() const noexcept
        {
            return !m_dependencyNotifiers.empty() || !m_autoExecuteNotifiers.empty();
        }

    #pragma endregion

    #pragma region instance fields
    private:
        template <typename E>
        static void ResetEventInPlace(E& e) noexcept
        {
            using std::destroy_at;
            using std::construct_at;
            destroy_at(std::addressof(e));   // 调用事件对象的析构函数，释放全部订阅
            construct_at(std::addressof(e)); // 默认构造一个全新的 event 对象
        }

        ExecuteHandler m_executeHandler;
        CanExecuteHandler m_canExecuteHandler;
        winrt::event< winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> > m_eventCanExecuteChanged;

        winrt::event< winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs> > m_eventCanExecuteRequested;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs> > m_eventCanExecuteCompleted;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs> > m_eventExecuteRequested;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs> > m_eventExecuteCompleted;
        
        std::vector< winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged> > m_dependencyNotifiers;
        std::vector< winrt::event_token > m_dependencyTokens;
        std::vector< winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged> > m_autoExecuteNotifiers;
        std::vector< winrt::event_token > m_autoExecuteTokens;
    #pragma endregion
    };
}

#endif // __MVVM_CPPWINRT_DELEGATE_COMMAND_H_INCLUDED
