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
#ifndef __MVVM_CPPWINRT_NOTIFY_PROPERTY_CHANGED_H_INCLUDED
#define __MVVM_CPPWINRT_NOTIFY_PROPERTY_CHANGED_H_INCLUDED

#include <algorithm>
#include <initializer_list>
#include <type_traits>

#include <winrt/Microsoft.UI.Xaml.Data.h>

#include "name_of.h"

namespace mvvm
{
    template <typename Derived>
    struct __declspec(empty_bases) notify_property_changed
    {
        friend typename Derived;

    #pragma region INotifyPropertyChanged
        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            m_propertyChangedToken = m_eventPropertyChanged.add(handler);
            return m_propertyChangedToken;
        }
        void PropertyChanged(winrt::event_token token)
        {
            m_eventPropertyChanged.remove(token);
        }
    private:
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_eventPropertyChanged;
        winrt::event_token m_propertyChangedToken{};
    #pragma endregion

    protected:
        static constexpr std::nullptr_t nullptr_ref{};
        notify_property_changed()
        {
            // This is used to ensure that the derived class is actually derived from notify_property_changed
            // and not just a base class of it.
            static_assert(std::is_base_of_v<notify_property_changed<Derived>, Derived>);
        }

        ~notify_property_changed()
        {
            if (m_propertyChangedToken)
            {
                m_eventPropertyChanged.remove(m_propertyChangedToken);
            }
        }

    #pragma region get_property

        template <typename Value>
        Value get_property_core(Value const& valueField)
        {
            return valueField;
        }

        template <typename Value>
        Value get_property_core(Value const& valueField) const
        {
            return valueField;
        }

        template <typename Value>
        Value get_property(Value const& valueField)
        {
            return derived().get_property_core(valueField);
        }

        template <typename Value>
        Value get_property(Value const& valueField) const
        {
            return derived().get_property_core(valueField);
        }

    #pragma endregion

    #pragma region set_property: notifying

        template <typename Value>
        bool set_property(Value& valueField, Value const& newValue, std::wstring_view const& propertyName)
        {
            return derived().set_property_override<Value, const std::nullptr_t, true, decltype(propertyName)>(valueField, newValue, nullptr_ref, propertyName);
        }

        template <typename Value>
        bool set_property(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName)
        {
            return derived().set_property_override<Value, Value, true, decltype(propertyName)>(valueField, newValue, oldValue, propertyName);
        }

        template <typename Value>
        bool set_property(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            return derived().set_property_override<Value, const std::nullptr_t, true, decltype(propertyNames)>(valueField, newValue, nullptr_ref, propertyNames);
        }

        template <typename Value>
        bool set_property(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            return derived().set_property_override<Value, Value, true, decltype(propertyNames)>(valueField, newValue, oldValue, propertyNames);
        }

    #pragma endregion

    #pragma region set_property: non-notifying

        template <typename Value>
        bool set_property(Value& valueField, Value const& newValue)
        {
            return derived().set_property_override<Value, const std::nullptr_t, true, const std::nullptr_t>(valueField, newValue, nullptr_ref, nullptr_ref);
        }

        template <typename Value>
        bool set_property(Value& valueField, Value const& newValue, Value& oldValue)
        {
            return derived().set_property_override<Value, Value, true, const std::nullptr_t>(valueField, newValue, oldValue, nullptr_ref);
        }

    #pragma endregion

    #pragma region set_property_no_compare: notifying

        template <typename Value>
        void set_property_no_compare(Value& valueField, Value const& newValue, std::wstring_view const& propertyName)
        {
            derived().set_property_override<Value, const std::nullptr_t, false, decltype(propertyName)>(valueField, newValue, nullptr_ref, propertyName);
        }

        template <typename Value>
        void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName)
        {
            derived().set_property_override<Value, Value, false, decltype(propertyName)>(valueField, newValue, oldValue, propertyName);
        }

        template <typename Value>
        void set_property_no_compare(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            derived().set_property_override<Value, const std::nullptr_t, false, decltype(propertyNames)>(valueField, newValue, nullptr_ref, propertyNames);
        }

        template <typename Value>
        void set_property_no_compare(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            derived().set_property_override<Value, Value, false, decltype(propertyNames)>(valueField, newValue, oldValue, propertyNames);
        }

    #pragma endregion

    #pragma region set_property_no_compare: non-notifying

        template <typename Value>
        void set_property_no_compare_no_notify(Value& valueField, Value const& newValue)
        {
            derived().set_property_override<Value, const std::nullptr_t, false, const std::nullptr_t>(valueField, newValue, nullptr_ref, nullptr_ref);
        }

        template <typename Value>
        void set_property_no_compare_no_notify(Value& valueField, Value const& newValue, Value& oldValue)
        {
            derived().set_property_override<Value, Value, false, const std::nullptr_t>(valueField, newValue, oldValue, nullptr_ref);
        }

    #pragma endregion

        template <typename Value, typename OldValue, bool compare, typename PropertyName>
        bool set_property_core(
            Value& valueField,
            Value const& newValue,
            [[maybe_unused]] OldValue& oldValue,
            [[maybe_unused]] PropertyName const& propertyNameOrNames)
        {
            constexpr bool isOldValueTypeNull = std::is_null_pointer_v<OldValue>;
            constexpr bool isOldValueTypeSameAsValue = std::is_same_v<OldValue, Value>;
            static_assert(isOldValueTypeNull || isOldValueTypeSameAsValue);

            constexpr bool isPropertyNameNull = std::is_null_pointer_v<PropertyName>;
            constexpr bool isPropertyNameSingle = std::is_convertible_v<PropertyName, const std::wstring_view>;
            constexpr bool isPropertyNameMultiple = std::is_convertible_v<PropertyName, std::initializer_list<const std::wstring_view>>;
            static_assert(isPropertyNameNull || isPropertyNameSingle || isPropertyNameMultiple);

            if constexpr (!isOldValueTypeNull)
            {
                oldValue = valueField;
            }

            bool valueChanged = true;

            if constexpr (compare)
            {
                valueChanged = valueField != newValue;
                if (valueChanged)
                {
                    valueField = newValue;
                }
            }
            else
            {
                valueField = newValue;
            }

            if constexpr (!isPropertyNameNull)
            {
                if (valueChanged && m_eventPropertyChanged)
                {
                    raise_PropertyChanged(propertyNameOrNames);
                }
            }

            return valueChanged;
        }

        Derived& derived()
        {
            return static_cast<Derived&>(*this);
        }

        Derived const& derived() const
        {
            return static_cast<Derived const&>(*this);
        }

    public:
        template <typename Value>
        Value get_property_override(Value const& valueField)
        {
            return derived().get_property_core(valueField);
        }

        template <typename Value, typename OldValue, bool compare, typename PropertyName>
        bool set_property_override(
            Value& valueField,
            Value const& newValue,
            [[maybe_unused]] OldValue& oldValue,
            [[maybe_unused]] PropertyName const& propertyNameOrNames)
        {
            return set_property_core(valueField, newValue, oldValue, propertyNameOrNames);
        }

        void raise_PropertyChanged(std::wstring_view const& propertyName)
        {
            // Only instantiate the arguments class if the event has any listeners
            if (m_eventPropertyChanged)
            {
                winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs args{ propertyName };
                m_eventPropertyChanged(derived(), args);
            }
        }

        void raise_PropertyChanged(std::initializer_list<const std::wstring_view> const& propertyNames)
        {
            // Only instantiate the argumens class (and only once) if the event has any listeners
            if (m_eventPropertyChanged)
            {
                for (auto&& propertyName : propertyNames)
                {
                    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs args{ propertyName };
                    m_eventPropertyChanged(derived(), args);
                }
            }
        }
    };
}

#endif // __MVVM_CPPWINRT_NOTIFY_PROPERTY_CHANGED_H_INCLUDED
