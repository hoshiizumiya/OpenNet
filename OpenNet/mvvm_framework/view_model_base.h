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
#ifndef __MVVM_CPPWINRT_VIEW_MODEL_BASE_H_INCLUDED
#define __MVVM_CPPWINRT_VIEW_MODEL_BASE_H_INCLUDED

#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include "notify_property_changed.h"

namespace mvvm
{
    template <typename Derived>
    struct __declspec(empty_bases)ViewModelBase : WrapNotifyPropertyChanged<Derived>
    {
        friend typename Derived;

        template <typename TValue>
        inline TValue GetPropertyOverride(TValue const& valueField)
        {
            if (this->derived().HasThreadAccess())
            {
                return base::notify_property_changed::GetPropertyCore(valueField);
            }

            auto dispatcher = this->derived().Dispatcher();
            if (!dispatcher)
                return valueField;

            winrt::Windows::Foundation::IAsyncOperation<TValue> operation;
            dispatcher.TryEnqueue([&]()
                {
                    operation = []() -> winrt::Windows::Foundation::IAsyncOperation<TValue>
                        {
                            co_return base::notify_property_changed::GetPropertyCore(valueField);
                        }();
                });
            return operation.get();
        }

        template <typename TValue, typename TOldValue, bool compare, typename propertyNameType>
        inline bool SetPropertyOverride(TValue& valueField, TValue const& newValue, TOldValue& oldValue, propertyNameType const& propertyNameOrNames)
        {            
            if (this->derived().HasThreadAccess())
            {
                return this->SetPropertyCore<TValue, TOldValue, compare, propertyNameType>(std::forward<TValue&>(valueField), newValue, oldValue, propertyNameOrNames);
            }

            auto dispatcher = this->derived().GetDispatcherOverride();
            if (!dispatcher)
                return false;

            winrt::Windows::Foundation::IAsyncOperation<bool> operation;
            dispatcher.TryEnqueue([&]()
                {
                    operation = [&]() -> winrt::Windows::Foundation::IAsyncOperation<bool>
                        {
                            co_return this->SetPropertyCore<TValue, TOldValue, compare, propertyNameType>(
                                std::forward<TValue&>(valueField), newValue, oldValue, propertyNameOrNames);
                        }();
                });

            return operation.get();
        }

        winrt::Microsoft::UI::Dispatching::DispatcherQueue GetDispatcherOverride() { return { nullptr }; }

        // UI thread HTA check
        bool HasThreadAccess() const
        {
            auto dispatcher = this->derived().Dispatcher();
            if (!dispatcher)
                return false;

            // Gets a value indicating whether the DispatcherQueue has access to the current thread.
            if (dispatcher.HasThreadAccess())
                return true;

            // Compare the current thread's DispatcherQueue
            auto currentDispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            return currentDispatcher && (currentDispatcher == dispatcher);
        }

        void IsThreadAccessible() const
        {
            if (!HasThreadAccess())
            {
                throw winrt::hresult_wrong_thread();
            }
        }

    protected:
        // This is used to ensure that the derived class is actually derived from ViewModelBase
        ViewModelBase()
        {
            static_assert(std::is_base_of_v<ViewModelBase, Derived>, "Derived class must inherit from ViewModelBase");
        }
        using base = typename ::mvvm::WrapNotifyPropertyChanged<Derived>;
    };
}

#endif // __MVVM_CPPWINRT_VIEW_MODEL_BASE_H_INCLUDED
