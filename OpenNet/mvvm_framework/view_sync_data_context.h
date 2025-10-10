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
#ifndef __MVVM_CPPWINRT_VIEW_SYNC_DATA_CONTEXT_H_INCLUDED
#define __MVVM_CPPWINRT_VIEW_SYNC_DATA_CONTEXT_H_INCLUDED

#include <string_view>
#include <wil\resource.h>
#include "view_model_base.h"

namespace mvvm
{
    using namespace std::literals;

    template <typename Derived, typename ViewModelT>
    struct __declspec(empty_bases) view_sync_data_context
        : ViewModelBase<Derived>
    {
        using view_model_type = typename ViewModelT;
        using view_base_type = typename view_sync_data_context<Derived, ViewModelT>;

        friend typename Derived;

        view_sync_data_context()
        {
            // Setup the ViewModel property to shadow (synchronize with) the DataContext property
            this->derived().DataContextChanged([this, weakThis = this->derived().get_weak()](auto&&, auto&& args)
            {
                if (auto thisRef = weakThis.get())
                {
                    auto resetRecurseGuard = wil::scope_exit([this, thisRef]
                    {
                        m_isDataContextUpdating = false;
                    });
                    m_isDataContextUpdating = true;

                    if (!m_isViewModelUpdating)
                    {
                        this->derived().ViewModel(args.NewValue().as<view_model_type>());
                    }

                    this->derived().Bindings->Update();
                }
            });
        }

        view_sync_data_context(view_model_type const& viewModel)
        {
            this->derived().DataContext(viewModel);
        }

        view_model_type ViewModel()
        {
            auto dataContext = this->derived().DataContext();
            return dataContext.try_as<view_model_type>();
        }
        void ViewModel(view_model_type const& value)
        {
            auto resetRecurseGuard = wil::scope_exit([this]
            {
                m_isViewModelUpdating = false;
            });
            m_isViewModelUpdating = true;

            if (!m_isDataContextUpdating)
            {
                this->derived().DataContext(value);
            }
        };

    private:
        bool m_isViewModelUpdating : 1 {};
        bool m_isDataContextUpdating : 1 {};
    };
}

#endif // __MVVM_CPPWINRT_VIEW_SYNC_DATA_CONTEXT_H_INCLUDED
