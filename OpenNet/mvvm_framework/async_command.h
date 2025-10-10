#pragma once
#ifndef __MVVM_CPPWINRT_ASYNC_DELEGATE_COMMAND_H_INCLUDED
#define __MVVM_CPPWINRT_ASYNC_DELEGATE_COMMAND_H_INCLUDED

#include <functional>
#include <type_traits>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>

#include <mvvm_framework/mvvm_framework_events.h>  // Can/Execute EventArgs (same as sync)
#include <mvvm_framework/mvvm_diagnostics.h>     // optional
#include "mvvm_framework/mvvm_hresult_helper.h"

namespace mvvm
{
    using RelayDependencyCondition = std::function<bool(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const&)>;

    using AutoExecuteCondition = std::function<bool(
        winrt::Windows::Foundation::IInspectable const&)>;

    // =========================================================================================
    //  Helpers for parameter dispatching (same rules as delegate_command)
    // =========================================================================================
    template<typename Parameter, typename Fn>
    inline auto SmartInvoke(Fn&& fn,
        winrt::Windows::Foundation::IInspectable const& parameter)
    {
        using NakedParameterType = std::conditional_t<std::is_same_v<Parameter, void>, void,
            std::remove_const_t<std::remove_reference_t<Parameter>>>;

        if constexpr (std::is_same_v<Parameter, void>)
        {
            return std::invoke(std::forward<Fn>(fn));
        }
        else if constexpr (std::is_same_v<NakedParameterType, winrt::Windows::Foundation::IInspectable>)
        {
            return std::invoke(std::forward<Fn>(fn), parameter);
        }
        else if constexpr (std::is_convertible_v<NakedParameterType, winrt::Windows::Foundation::IInspectable>)
        {
            return std::invoke(std::forward<Fn>(fn), parameter.try_as<NakedParameterType>());
        }
        else
        {
            return std::invoke(std::forward<Fn>(fn),
                winrt::unbox_value_or<NakedParameterType>(parameter, {}));
        }
    }

    // =========================================================================================
    //  AsyncDelegateCommand<Parameter>  -> IAsyncAction
    // =========================================================================================
    template <typename Parameter = void>
    struct AsyncDelegateCommand
        : winrt::implements<AsyncDelegateCommand<Parameter>
            , winrt::Microsoft::UI::Xaml::Input::ICommand
            , winrt::Mvvm::Framework::Core::ICommandCleanup>
    {
        using ExecuteAsyncHandler = std::function<winrt::Windows::Foundation::IAsyncAction(
            std::add_lvalue_reference_t<std::conditional_t<std::is_same_v<Parameter, void>, void,
            std::add_const_t<std::remove_reference_t<Parameter>>>>)>;

        using CanExecuteHandler = std::function<bool(
            std::add_lvalue_reference_t<std::conditional_t<std::is_same_v<Parameter, void>, void,
            std::add_const_t<std::remove_reference_t<Parameter>>>>)>;

        // ------------------------------------------------------------
        //  Ctors / Dtor
        // ------------------------------------------------------------
        AsyncDelegateCommand() = default;

        explicit AsyncDelegateCommand(ExecuteAsyncHandler executeAsync)
            : m_executeAsync(std::move(executeAsync)) {
        }

        AsyncDelegateCommand(ExecuteAsyncHandler executeAsync, CanExecuteHandler canExecute)
            : m_executeAsync(std::move(executeAsync)), m_canExecute(std::move(canExecute)) {
        }

        template <typename ExecT, typename CanT>
        AsyncDelegateCommand(
            winrt::Windows::Foundation::IInspectable const& notifier,
            ExecT&& exec, CanT&& can,
            std::vector<DependencyRegistration> const& dependencies)
            : m_executeAsync(std::forward<ExecT>(exec)),
            m_canExecute(std::forward<CanT>(can))
        {
            AttachDependencies(notifier, dependencies);
        }

        ~AsyncDelegateCommand()
        {
            // detach dependency listeners
            for (size_t i = 0; i < m_dependencyNotifiers.size(); ++i)
            {
                if (auto inpc = m_dependencyNotifiers[i].get())
                    inpc.PropertyChanged(m_dependencyTokens[i]);
            }
            for (size_t i = 0; i < m_autoExecuteNotifiers.size(); ++i)
            {
                if (auto inpc = m_autoExecuteNotifiers[i].get())
                    inpc.PropertyChanged(m_autoExecuteTokens[i]);
            }
        }

        // ------------------------------------------------------------
        //  ICommand
        // ------------------------------------------------------------
        winrt::event_token CanExecuteChanged(
            winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& h)
        {
            return m_canExecuteChanged.add(h);
        }

        void CanExecuteChanged(winrt::event_token const& t) { m_canExecuteChanged.remove(t); }

        // CanExecuteRequested/Completed
        winrt::event_token CanExecuteRequested(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs> const& h)
        {
            return m_evtCanReq.add(h);
        }

        void CanExecuteRequested(winrt::event_token const& t) { m_evtCanReq.remove(t); }

        winrt::event_token CanExecuteCompleted(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs> const& h)
        {
            return m_evtCanCpl.add(h);
        }

        void CanExecuteCompleted(winrt::event_token const& t) { m_evtCanCpl.remove(t); }

        // ExecuteRequested/Completed
        winrt::event_token ExecuteRequested(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs> const& h)
        {
            return m_evtExecReq.add(h);
        }

        void ExecuteRequested(winrt::event_token const& t) { m_evtExecReq.remove(t); }

        winrt::event_token ExecuteCompleted(
            winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs> const& h)
        {
            return m_evtExecCpl.add(h);
        }

        void ExecuteCompleted(winrt::event_token const& t) { m_evtExecCpl.remove(t); }

        winrt::event_token ExecutionStateChanged(auto const& h) { return m_evtStateChanged.add(h); }
        void ExecutionStateChanged(winrt::event_token const& t) { m_evtStateChanged.remove(t); }

        void SetObserver(winrt::Mvvm::Framework::Core::IAsyncCommandObserver const& o)
        {
            m_observer = winrt::make_weak(o);
        }

        void NotifyState(winrt::Windows::Foundation::IInspectable const& parameter,
            winrt::Mvvm::Framework::Core::ExecutionState s, int32_t hr = S_OK)
        {
            if (m_evtStateChanged)
                m_evtStateChanged(*this, winrt::Mvvm::Framework::Core::ExecutionStateChangedEventArgs(parameter, s, hr));

            if (auto obs = m_observer.get())
                obs.OnExecutionStateChanged(winrt::Mvvm::Framework::Core::ExecutionStateChangedEventArgs(parameter, s, hr));
        }

        bool CanExecute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            // Requested
            if (m_evtCanReq)
                m_evtCanReq(*this, winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs(parameter));

            // 没有执行处理器，则默认认为不可以执行
            bool ok = (m_executeAsync != nullptr);

            // 运行中禁用（可选允许重入）
            if (ok && m_isRunning && !m_allowReentrancy) ok = false;

            if (ok && m_canExecute)
            {
                ok = SmartInvoke<Parameter>(m_canExecute, parameter);
            }

            // Completed
            if (m_evtCanCpl)
                m_evtCanCpl(*this, winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs(parameter, ok));

            return ok;
        }

        void Execute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            if (!m_executeAsync) return;

            if (m_evtExecReq)
                m_evtExecReq(*this, winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs(parameter));

            // 进入运行状态，通知可执行状态变化
            m_isRunning = true;
            RaiseCanExecuteChangedEvent();
            NotifyState(parameter, winrt::Mvvm::Framework::Core::ExecutionState::Started);

            winrt::hresult hr = S_OK;
            
            try
            {
                m_runningAction = SmartInvoke<Parameter>(m_executeAsync, parameter);

                // 完成回调：设置状态为完成态 Completed（不使用 co_await 以免捕获上下文）
                auto weak = this->get_weak();
                m_runningAction.Completed([weak, parameter](
                    winrt::Windows::Foundation::IAsyncAction const& action,
                    winrt::Windows::Foundation::AsyncStatus const status)
                    {
                        if (auto self = weak.get())
                        {
                            // 结束运行，通知可执行状态变化
                            self->m_isRunning = false;
                            self->RaiseCanExecuteChangedEvent();

                            // 将 AsyncStatus 映射到 HRESULT
                            int32_t hrLocal = S_OK;
                            auto st = winrt::Mvvm::Framework::Core::ExecutionState::Succeeded;
                            if (status == winrt::Windows::Foundation::AsyncStatus::Canceled)
                            {
                                hrLocal = mvvm::HResultHelper::hresult_error_fCanceled();
                                st = winrt::Mvvm::Framework::Core::ExecutionState::Canceled;
                            }
                            else if (status == winrt::Windows::Foundation::AsyncStatus::Error)
                            {
                                hrLocal = static_cast<int32_t>(action.ErrorCode().value);
                                st = winrt::Mvvm::Framework::Core::ExecutionState::Failed;
                            }

                            self->NotifyState(parameter, st, hrLocal);

                            // 通知执行完成事件（由订阅方自行决定是否切回 UI）
                            if (self->m_evtExecCpl)
                            {
                                self->m_evtExecCpl(
                                    *self,
                                    winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs(parameter, hrLocal));
                            }
                        }
                    });
            }
            catch (winrt::hresult_error const& e)
            {
                hr = e.code();
            }
            catch (...)
            {
                hr = E_FAIL;
            }

            if (FAILED(hr))
            {
                m_isRunning = false;
                RaiseCanExecuteChangedEvent();
                if (m_evtExecCpl)
                    m_evtExecCpl(*this, winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs(parameter, hr));
            }
        }

        void RaiseCanExecuteChangedEvent()
        {
            if (m_canExecuteChanged) m_canExecuteChanged(*this, nullptr);
        }

        // ------------------------------------------------------------
        //  Cancellation & Options
        // ------------------------------------------------------------
        void Cancel() noexcept
        {
            try
            {
                if (m_runningAction) m_runningAction.Cancel();
            }
            catch (...) {}
            NotifyState(winrt::box_value({}),
                winrt::Mvvm::Framework::Core::ExecutionState::Cancelling);
        }

        void AllowReentrancy(bool value) noexcept { m_allowReentrancy = value; }
        bool AllowReentrancy() const noexcept { return m_allowReentrancy; }

        bool IsRunning() const noexcept { return m_isRunning; }

        // ------------------------------------------------------------
        //  Dependencies & Auto-exec (same behavior as sync)
        // ------------------------------------------------------------
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
                    [weakThis, prop = propertyName, cond = std::move(condition)]
                    (auto&& sender, auto&& args)
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

        void RegisterAutoExecute(
            winrt::Windows::Foundation::IInspectable const& notifier,
            AutoExecuteCondition condition)
        {
            if (auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>())
            {
                auto weakThis = this->get_weak();
                auto token = inpc.PropertyChanged(
                    [weakThis, cond = std::move(condition)]
                    (auto&& sender, auto const&)
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
                        RegisterAutoExecute(notifier, dep.autoExecuteCondition);
                }
            }
        }

        // 取消注册（Detach/Unregister）

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

        // 重置处理器 / 清空订阅者

        // 清空 Execute/CanExecute 的委托
        void ResetHandlers() noexcept
        {
            m_executeAsync = {};
            m_canExecute = {};
        }

        // 清空命令外部订阅事件的订阅者（CanExecuteChanged/Requested/...）
        void ClearAllSubscribers() noexcept
        {
            ResetEventInPlace(m_canExecuteChanged);
            ResetEventInPlace(m_evtCanReq);
            ResetEventInPlace(m_evtCanCpl);
            ResetEventInPlace(m_evtExecReq);
            ResetEventInPlace(m_evtExecCpl);
        }

        // 判断是否有依赖（RelayDependency/AutoExecute）
        bool HasDependencies() const noexcept
        {
            return !m_dependencyNotifiers.empty() || !m_autoExecuteNotifiers.empty();
        }

    private:
        template <typename E>
        static void ResetEventInPlace(E& e) noexcept
        {
            using std::destroy_at;
            using std::construct_at;
            destroy_at(std::addressof(e));   // 调用事件对象的析构函数，释放全部订阅
            construct_at(std::addressof(e)); // 默认构造一个全新的 event 对象
        }

        ExecuteAsyncHandler  m_executeAsync;
        CanExecuteHandler    m_canExecute;

        bool m_isRunning{ false };
        bool m_allowReentrancy{ false };

        winrt::Windows::Foundation::IAsyncAction m_runningAction{ nullptr };

        // events
        winrt::event< winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> > m_canExecuteChanged;

        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs> > m_evtCanReq;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs> > m_evtCanCpl;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs> > m_evtExecReq;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs> > m_evtExecCpl;

        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::ExecutionStateChangedEventArgs> > m_evtStateChanged;

        winrt::weak_ref<winrt::Mvvm::Framework::Core::IAsyncCommandObserver> m_observer;

        // dependency trackers
        std::vector< winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged> > m_dependencyNotifiers;
        std::vector< winrt::event_token > m_dependencyTokens;
        std::vector< winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged> > m_autoExecuteNotifiers;
        std::vector< winrt::event_token > m_autoExecuteTokens;
    };

    // =========================================================================================
    //  AsyncDelegateCommand<Parameter, TResult>  -> IAsyncOperation<TResult>
    // =========================================================================================
    template <typename Parameter, typename TResult>
    struct AsyncDelegateCommandResult
        : winrt::implements<AsyncDelegateCommandResult<Parameter, TResult>
            , winrt::Microsoft::UI::Xaml::Input::ICommand
            , winrt::Mvvm::Framework::Core::ICommandCleanup>
    {
        using ExecuteAsyncHandler = std::function<winrt::Windows::Foundation::IAsyncOperation<TResult>(
            std::add_lvalue_reference_t<std::conditional_t<std::is_same_v<Parameter, void>, void,
            std::add_const_t<std::remove_reference_t<Parameter>>>>)>;

        using CanExecuteHandler = std::function<bool(
            std::add_lvalue_reference_t<std::conditional_t<std::is_same_v<Parameter, void>, void,
            std::add_const_t<std::remove_reference_t<Parameter>>>>)>;

        AsyncDelegateCommandResult() = default;

        explicit AsyncDelegateCommandResult(ExecuteAsyncHandler exec) : m_executeAsync(std::move(exec)) {}
        AsyncDelegateCommandResult(ExecuteAsyncHandler exec, CanExecuteHandler can)
            : m_executeAsync(std::move(exec)), m_canExecute(std::move(can)) {
        }

        template<typename ExecT, typename CanT>
        AsyncDelegateCommandResult(
            winrt::Windows::Foundation::IInspectable const& notifier,
            ExecT&& exec, CanT&& can,
            std::vector<DependencyRegistration> const& deps)
            : m_executeAsync(std::forward<ExecT>(exec)),
            m_canExecute(std::forward<CanT>(can))
        {
            AttachDependencies(notifier, deps);
        }

        ~AsyncDelegateCommandResult()
        {
            for (size_t i = 0; i < m_dependencyNotifiers.size(); ++i)
                if (auto inpc = m_dependencyNotifiers[i].get()) inpc.PropertyChanged(m_dependencyTokens[i]);
            for (size_t i = 0; i < m_autoExecuteNotifiers.size(); ++i)
                if (auto inpc = m_autoExecuteNotifiers[i].get()) inpc.PropertyChanged(m_autoExecuteTokens[i]);
        }

        // ICommand
        winrt::event_token CanExecuteChanged(
            winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& h)
        {
            return m_canExecuteChanged.add(h);
        }

        void CanExecuteChanged(winrt::event_token const& t) { m_canExecuteChanged.remove(t); }

        // same events as above
        winrt::event_token CanExecuteRequested(auto const& h) { return m_evtCanReq.add(h); }
        void CanExecuteRequested(winrt::event_token const& t) { m_evtCanReq.remove(t); }
        winrt::event_token CanExecuteCompleted(auto const& h) { return m_evtCanCpl.add(h); }
        void CanExecuteCompleted(winrt::event_token const& t) { m_evtCanCpl.remove(t); }

        winrt::event_token ExecuteRequested(auto const& h) { return m_evtExecReq.add(h); }
        void ExecuteRequested(winrt::event_token const& t) { m_evtExecReq.remove(t); }
        winrt::event_token ExecuteCompleted(auto const& h) { return m_evtExecCpl.add(h); }
        void ExecuteCompleted(winrt::event_token const& t) { m_evtExecCpl.remove(t); }

        winrt::event_token ExecutionStateChanged(auto const& h) { return m_evtStateChanged.add(h); }
        void ExecutionStateChanged(winrt::event_token const& t) { m_evtStateChanged.remove(t); }

        void SetObserver(winrt::Mvvm::Framework::Core::IAsyncCommandObserver const& o)
        {
            m_observer = winrt::make_weak(o);
        }

        void NotifyState(winrt::Windows::Foundation::IInspectable const& parameter,
            winrt::Mvvm::Framework::Core::ExecutionState s, int32_t hr = S_OK)
        {
            if (m_evtStateChanged)
                m_evtStateChanged(*this, winrt::Mvvm::Framework::Core::ExecutionStateChangedEventArgs(parameter, s, hr));

            if (auto obs = m_observer.get())
                obs.OnExecutionStateChanged(winrt::Mvvm::Framework::Core::ExecutionStateChangedEventArgs(parameter, s, hr));
        }

        bool CanExecute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            if (m_evtCanReq)
                m_evtCanReq(*this, winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs(parameter));

            // 如果没有没有处理函数，则认为不可以执行
            if (m_executeAsync == nullptr)
            {
                return false;
            }

            bool ok = !(m_isRunning && !m_allowReentrancy);
            if (ok && m_canExecute)
                ok = SmartInvoke<Parameter>(m_canExecute, parameter);

            if (m_evtCanCpl)
                m_evtCanCpl(*this, winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs(parameter, ok));
            return ok;
        }

        void Execute(winrt::Windows::Foundation::IInspectable const& parameter)
        {
            if (!m_executeAsync) return;

            if (m_evtExecReq)
                m_evtExecReq(*this, winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs(parameter));

            m_isRunning = true;
            RaiseCanExecuteChangedEvent();
            NotifyState(parameter, winrt::Mvvm::Framework::Core::ExecutionState::Started);

            winrt::hresult hr = S_OK;
            try
            {
                m_runningOp = SmartInvoke<Parameter>(m_executeAsync, parameter);

                auto weak = this->get_weak();
                m_runningOp.Completed([weak, parameter](
                    winrt::Windows::Foundation::IAsyncOperation<TResult> const& op,
                    winrt::Windows::Foundation::AsyncStatus const status)
                    {
                        if (auto self = weak.get())
                        {
                            self->m_isRunning = false;
                            self->RaiseCanExecuteChangedEvent();

                            int32_t hrLocal = S_OK;
                            auto st = winrt::Mvvm::Framework::Core::ExecutionState::Succeeded;
                            if (status == winrt::Windows::Foundation::AsyncStatus::Canceled)
                            {
                                hrLocal = mvvm::HResultHelper::hresult_error_fCanceled();
                                st = winrt::Mvvm::Framework::Core::ExecutionState::Canceled;
                            }
                            else if (status == winrt::Windows::Foundation::AsyncStatus::Error)
                            {
                                hrLocal = static_cast<int32_t>(op.ErrorCode().value);
                                st = winrt::Mvvm::Framework::Core::ExecutionState::Failed;
                            }

                            self->NotifyState(parameter, st, hrLocal);

                            if (self->m_evtExecCpl)
                            {
                                self->m_evtExecCpl(*self,
                                    winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs(parameter, hrLocal));
                            }
                        }
                    });
            }
            catch (winrt::hresult_error const& e) { hr = e.code(); }
            catch (...) { hr = E_FAIL; }

            if (FAILED(hr))
            {
                m_isRunning = false;
                RaiseCanExecuteChangedEvent();
                if (m_evtExecCpl)
                    m_evtExecCpl(*this, winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs(parameter, hr));
            }
        }

        void RaiseCanExecuteChangedEvent()
        {
            if (m_canExecuteChanged) m_canExecuteChanged(*this, nullptr);
        }

        void Cancel() noexcept
        {
            try { if (m_runningOp) m_runningOp.Cancel(); }
            catch (...) {}
            NotifyState(winrt::box_value({}),
                winrt::Mvvm::Framework::Core::ExecutionState::Cancelling);
        }

        void AllowReentrancy(bool v) noexcept { m_allowReentrancy = v; }
        bool AllowReentrancy() const noexcept { return m_allowReentrancy; }

        bool IsRunning() const noexcept { return m_isRunning; }

        // Dependencies & Auto-exec (same as above)
        void OnAttachPropertyChanged(
            winrt::hstring const& prop, RelayDependencyCondition const& cond,
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
        {
            if (prop.empty() || args.PropertyName() == prop)
            {
                if (!cond || cond(sender, args))
                    RaiseCanExecuteChangedEvent();
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
                    [weakThis, prop = propertyName, cond = std::move(condition)]
                    (auto&& sender, auto&& args)
                    {
                        if (auto self = weakThis.get())
                            self->OnAttachPropertyChanged(prop, cond, sender, args);
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
                Execute(winrt::Windows::Foundation::IInspectable{ nullptr });
        }

        void RegisterAutoExecute(
            winrt::Windows::Foundation::IInspectable const& notifier,
            AutoExecuteCondition condition)
        {
            if (auto inpc = notifier.try_as<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>())
            {
                auto weakThis = this->get_weak();
                auto token = inpc.PropertyChanged(
                    [weakThis, cond = std::move(condition)]
                    (auto&& sender, auto const&)
                    {
                        if (auto self = weakThis.get())
                            self->OnAutoExecuteCondChanged(cond, sender);
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
                        RegisterAutoExecute(notifier, dep.autoExecuteCondition);
                }
            }
        }

        // 取消注册（Detach/Unregister）
        
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

        // 重置处理器 / 清空订阅者

        // 清空 Execute/CanExecute 的委托
        void ResetHandlers() noexcept
        {
            m_executeAsync = {};
            m_canExecute = {};
        }

        // 清空命令外部订阅事件的订阅者（CanExecuteChanged/Requested/...）
        void ClearAllSubscribers() noexcept
        {
            ResetEventInPlace(m_canExecuteChanged);
            ResetEventInPlace(m_evtCanReq);
            ResetEventInPlace(m_evtCanCpl);
            ResetEventInPlace(m_evtExecReq);
            ResetEventInPlace(m_evtExecCpl);
        }

        // 判断是否有依赖（RelayDependency/AutoExecute）
        bool HasDependencies() const noexcept
        {
            return !m_dependencyNotifiers.empty() || !m_autoExecuteNotifiers.empty();
        }

    private:
        template <typename E>
        static void ResetEventInPlace(E& e) noexcept
        {
            using std::destroy_at;
            using std::construct_at;
            destroy_at(std::addressof(e));   // 调用事件对象的析构函数，释放全部订阅
            construct_at(std::addressof(e)); // 默认构造一个全新的 event 对象
        }

        ExecuteAsyncHandler  m_executeAsync;
        CanExecuteHandler    m_canExecute;
        bool m_isRunning{ false };
        bool m_allowReentrancy{ false };

        winrt::Windows::Foundation::IAsyncOperation<TResult> m_runningOp{ nullptr };

        // events
        winrt::event< winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> > m_canExecuteChanged;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::CanExecuteRequestedEventArgs> > m_evtCanReq;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::CanExecuteCompletedEventArgs> > m_evtCanCpl;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::ExecuteRequestedEventArgs> > m_evtExecReq;
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable, winrt::Mvvm::Framework::Core::ExecuteCompletedEventArgs> > m_evtExecCpl;

        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::ExecutionStateChangedEventArgs> > m_evtStateChanged;

        winrt::weak_ref<winrt::Mvvm::Framework::Core::IAsyncCommandObserver> m_observer;

        // dependency trackers
        std::vector< winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged> > m_dependencyNotifiers;
        std::vector< winrt::event_token > m_dependencyTokens;
        std::vector< winrt::weak_ref<winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged> > m_autoExecuteNotifiers;
        std::vector< winrt::event_token > m_autoExecuteTokens;
    };

    // 别名
    template<typename Parameter, typename TResult>
    using AsyncDelegateCommandR = AsyncDelegateCommandResult<Parameter, TResult>;
}

#endif // __MVVM_CPPWINRT_ASYNC_DELEGATE_COMMAND_H_INCLUDED
