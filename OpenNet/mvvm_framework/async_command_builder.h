#pragma once
#ifndef __MVVM_CPPWINRT_ASYNC_DELEGATE_COMMAND_BUILDER_H_INCLUDED
#define __MVVM_CPPWINRT_ASYNC_DELEGATE_COMMAND_BUILDER_H_INCLUDED

#include "async_command.h"

namespace mvvm
{
    template<typename Parameter = void>
    struct AsyncCommandBuilder
    {
        using CommandT = mvvm::AsyncDelegateCommand<Parameter>;

        explicit AsyncCommandBuilder(winrt::Windows::Foundation::IInspectable const& notifier = nullptr)
            : m_notifier(notifier) {
        }

        template<typename ExecT>
        auto& ExecuteAsync(ExecT&& exec)
        {
            m_exec = std::forward<ExecT>(exec);
            return *this;
        }

        template<typename CanT>
        auto& CanExecute(CanT&& can)
        {
            m_can = std::forward<CanT>(can);
            return *this;
        }

        auto& DependsOn(winrt::hstring const& prop,
            RelayDependencyCondition relay = nullptr,
            AutoExecuteCondition autoExec = nullptr)
        {
            m_deps.push_back(mvvm::DependencyRegistration{ prop, std::move(relay), std::move(autoExec) });
            return *this;
        }

        auto& ObserveWith(winrt::Windows::Foundation::IInspectable const& o)
        {
            m_observer = o;
            return *this;
        }

        auto Build() const
        {
            auto cmd = winrt::make_self<CommandT>(
                m_exec, m_can);
            if (!m_deps.empty() && m_notifier)
                cmd->AttachDependencies(m_notifier, m_deps);
            if (auto obs = m_observer.try_as<winrt::Mvvm::Framework::Core::IAsyncCommandObserver>())
                cmd->SetObserver(obs);
            return cmd.as<winrt::Microsoft::UI::Xaml::Input::ICommand>();
        }

    private:
        winrt::Windows::Foundation::IInspectable m_notifier{ nullptr };
        winrt::Windows::Foundation::IInspectable m_observer{ nullptr };
        typename CommandT::ExecuteAsyncHandler    m_exec{ nullptr };
        typename CommandT::CanExecuteHandler      m_can{ nullptr };
        std::vector<mvvm::DependencyRegistration> m_deps;
    };

    // TResult 版本
    template<typename Parameter, typename TResult>
    struct AsyncCommandBuilderR
    {
        using CommandT = mvvm::AsyncDelegateCommandR<Parameter, TResult>;

        explicit AsyncCommandBuilderR(winrt::Windows::Foundation::IInspectable const& notifier = nullptr)
            : m_notifier(notifier) {
        }

        template<typename ExecT>
        auto& ExecuteAsync(ExecT&& exec) { m_exec = std::forward<ExecT>(exec); return *this; }

        template<typename CanT>
        auto& CanExecute(CanT&& can) { m_can = std::forward<CanT>(can); return *this; }

        auto& DependsOn(winrt::hstring const& prop,
            RelayDependencyCondition relay = nullptr,
            AutoExecuteCondition autoExec = nullptr)
        {
            m_deps.push_back(mvvm::DependencyRegistration{ prop, std::move(relay), std::move(autoExec) });
            return *this;
        }

        auto& ObserveWith(winrt::Windows::Foundation::IInspectable const& o)
        {
            m_observer = o;
            return *this;
        }

        auto Build() const
        {
            auto cmd = winrt::make_self<CommandT>(m_exec, m_can);
            if (!m_deps.empty() && m_notifier)
                cmd->AttachDependencies(m_notifier, m_deps);
            if (auto obs = m_observer.try_as<winrt::Mvvm::Framework::Core::IAsyncCommandObserver>())
                cmd->SetObserver(obs);
            return cmd.as<winrt::Microsoft::UI::Xaml::Input::ICommand>();
        }

    private:
        winrt::Windows::Foundation::IInspectable m_notifier{ nullptr };
        winrt::Windows::Foundation::IInspectable m_observer{ nullptr };
        typename CommandT::ExecuteAsyncHandler    m_exec{ nullptr };
        typename CommandT::CanExecuteHandler      m_can{ nullptr };
        std::vector<mvvm::DependencyRegistration> m_deps;
    };
}

#endif
