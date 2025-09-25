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
#pragma once
#ifndef __MVVM_CPPWINRT_DELEGATE_COMMAND_BUILDER_H_INCLUDED
#define __MVVM_CPPWINRT_DELEGATE_COMMAND_BUILDER_H_INCLUDED

#include <mvvm_framework/delegate_command.h>

namespace mvvm
{
    template<typename Parameter>
    class DelegateCommandBuilder
    {
    public:
        explicit DelegateCommandBuilder(winrt::Windows::Foundation::IInspectable const& notifier)
            : m_notifier(notifier)
        {
        }

        DelegateCommandBuilder& Execute(std::function<void(Parameter const&)> handler)
        {
            m_executeHandler = std::move(handler);
            return *this;
        }

        DelegateCommandBuilder& CanExecute(std::function<bool(Parameter const&)> handler)
        {
            m_canExecuteHandler = std::move(handler);
            return *this;
        }

        DelegateCommandBuilder& DependsOn(
            std::wstring_view propertyName,
            RelayDependencyCondition relay = nullptr,
            AutoExecuteCondition autoExec = nullptr)
        {
            if(propertyName.data() == nullptr) // check for null
            {
                throw std::invalid_argument("Property name cannot be nullptr.");
            }

            m_dependencies.push_back({
                winrt::hstring(propertyName),
                std::move(relay),
                std::move(autoExec)
                });
            return *this;
        }

        auto Build()
        {
            return winrt::make<delegate_command<Parameter>>(
                m_notifier,
                m_executeHandler,
                m_canExecuteHandler,
                std::move(m_dependencies)
            );
        }

    private:
        winrt::Windows::Foundation::IInspectable m_notifier;
        std::function<void(Parameter const&)> m_executeHandler;
        std::function<bool(Parameter const&)> m_canExecuteHandler;
        std::vector<DependencyRegistration> m_dependencies;
    };
}

#endif // __MVVM_CPPWINRT_DELEGATE_COMMAND_BUILDER_H_INCLUDED