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
#ifndef __MVVM_CPPWINRT_VIEW_H_INCLUDED
#define __MVVM_CPPWINRT_VIEW_H_INCLUDED

#include <string_view>
#include "name_of.h"
#include "view_model_base.h"

namespace mvvm
{
    using namespace std::literals;

    template <typename Derived, typename ViewModel>
    struct __declspec(empty_bases) view : view_model_base<Derived>
    {
        using view_model_type = typename ViewModel;

        friend typename Derived;

        view(view_model_type const& viewModel)
            : m_viewModel(viewModel)
        {
        }

        view_model_type ViewModel()
        {
            return base::get_property(m_viewModel);
        }
        void ViewModel(view_model_type const& value)
        {
            base::set_property(m_viewModel, value, NAME_OF(view, ViewModel));
        }

    protected:
        view()
        {
            static_assert(std::is_base_of_v<view_model_base<Derived>, Derived>, "Derived class must inherit from view_model_base");
        }

    private:
        view_model_type m_viewModel;
        using base = typename view_model_base<Derived>;
    };
}

#endif // __MVVM_CPPWINRT_VIEW_H_INCLUDED
