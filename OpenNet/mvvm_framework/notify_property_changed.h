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
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "name_of.h"
#include <mvvm_framework/mvvm_framework_events.h>

#include <winrt/Microsoft.UI.Xaml.Data.h>

namespace mvvm
{
    template <typename Derived>
    struct __declspec(empty_bases) WrapNotifyPropertyChanged
    {
        friend typename Derived;

    #pragma region NotifyPropertyChanged Common Functions
        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            m_propertyChangedToken = m_eventPropertyChanged.add(handler);
            return m_propertyChangedToken;
        }
        void PropertyChanged(winrt::event_token token)
        {
            m_eventPropertyChanged.remove(token);
        }

        winrt::event_token ValidationRequested(auto const& handler) { return m_eventValidationRequested.add(handler); }
        void ValidationRequested(winrt::event_token const& token) { m_eventValidationRequested.remove(token); }

        winrt::event_token ValidationCompleted(auto const& handler) { return m_eventValidationCompleted.add(handler); }
        void ValidationCompleted(winrt::event_token const& token) { m_eventValidationCompleted.remove(token); }

        winrt::event_token ErrorsChanged(auto const& handler) { return m_eventErrorsChanged.add(handler); }
        void ErrorsChanged(winrt::event_token const& token) { m_eventErrorsChanged.remove(token); }
    private:

        // INotifyPropertyChanged
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_eventPropertyChanged;
        winrt::event_token m_propertyChangedToken{};

        // IValidation/IErrors
        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::ValidationRequestedEventArgs> > m_eventValidationRequested;

        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::ValidationCompletedEventArgs> > m_eventValidationCompleted;

        winrt::event< winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Foundation::IInspectable,
            winrt::Mvvm::Framework::Core::ValidationErrorsChangedEventArgs> > m_eventErrorsChanged;
        
    #pragma endregion

    protected:
        static constexpr std::nullptr_t nullptr_ref{};
        WrapNotifyPropertyChanged()
        {
            // This is used to ensure that the derived class is actually derived from WrapNotifyPropertyChanged
            // and not just a base class of it.
            static_assert(std::is_base_of_v<WrapNotifyPropertyChanged<Derived>, Derived>);
        }

        ~WrapNotifyPropertyChanged()
        {
            if (m_propertyChangedToken)
            {
                m_eventPropertyChanged.remove(m_propertyChangedToken);
            }
        }

    #pragma region GetProperty

        template <typename Value>
        Value GetPropertyCore(Value const& valueField)
        {
            return valueField;
        }

        template <typename Value>
        Value GetPropertyCore(Value const& valueField) const
        {
            return valueField;
        }

        template <typename Value>
        Value GetProperty(Value const& valueField)
        {
            return derived().GetPropertyCore(valueField);
        }

        template <typename Value>
        Value GetProperty(Value const& valueField) const
        {
            return derived().GetPropertyCore(valueField);
        }

    #pragma endregion

    #pragma region SetProperty: notifying

        template <typename Value>
        bool SetProperty(Value& valueField, Value const& newValue, std::wstring_view const& propertyName)
        {
            return derived().SetPropertyOverride<Value, const std::nullptr_t, true, decltype(propertyName)>(valueField, newValue, nullptr_ref, propertyName);
        }

        template <typename Value>
        bool SetProperty(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName)
        {
            return derived().SetPropertyOverride<Value, Value, true, decltype(propertyName)>(valueField, newValue, oldValue, propertyName);
        }

        template <typename Value>
        bool SetProperty(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            return derived().SetPropertyOverride<Value, const std::nullptr_t, true, decltype(propertyNames)>(valueField, newValue, nullptr_ref, propertyNames);
        }

        template <typename Value>
        bool SetProperty(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            return derived().SetPropertyOverride<Value, Value, true, decltype(propertyNames)>(valueField, newValue, oldValue, propertyNames);
        }

    #pragma endregion

    #pragma region SetProperty: non-notifying

        template <typename Value>
        bool SetProperty(Value& valueField, Value const& newValue)
        {
            return derived().SetPropertyOverride<Value, const std::nullptr_t, true, const std::nullptr_t>(valueField, newValue, nullptr_ref, nullptr_ref);
        }

        template <typename Value>
        bool SetProperty(Value& valueField, Value const& newValue, Value& oldValue)
        {
            return derived().SetPropertyOverride<Value, Value, true, const std::nullptr_t>(valueField, newValue, oldValue, nullptr_ref);
        }

    #pragma endregion

    #pragma region SetPropertyNoCompare: notifying

        template <typename Value>
        void SetPropertyNoCompare(Value& valueField, Value const& newValue, std::wstring_view const& propertyName)
        {
            derived().SetPropertyOverride<Value, const std::nullptr_t, false, decltype(propertyName)>(valueField, newValue, nullptr_ref, propertyName);
        }

        template <typename Value>
        void SetPropertyNoCompare(Value& valueField, Value const& newValue, Value& oldValue, std::wstring_view const& propertyName)
        {
            derived().SetPropertyOverride<Value, Value, false, decltype(propertyName)>(valueField, newValue, oldValue, propertyName);
        }

        template <typename Value>
        void SetPropertyNoCompare(Value& valueField, Value const& newValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            derived().SetPropertyOverride<Value, const std::nullptr_t, false, decltype(propertyNames)>(valueField, newValue, nullptr_ref, propertyNames);
        }

        template <typename Value>
        void SetPropertyNoCompare(Value& valueField, Value const& newValue, Value& oldValue, std::initializer_list<const std::wstring_view> propertyNames)
        {
            derived().SetPropertyOverride<Value, Value, false, decltype(propertyNames)>(valueField, newValue, oldValue, propertyNames);
        }

    #pragma endregion

    #pragma region SetPropertyNoCompare: non-notifying

        template <typename Value>
        void SetPropertyStraightThrough(Value& valueField, Value const& newValue)
        {
            derived().SetPropertyOverride<Value, const std::nullptr_t, false, const std::nullptr_t>(valueField, newValue, nullptr_ref, nullptr_ref);
        }

        template <typename Value>
        void SetPropertyStraightThrough(Value& valueField, Value const& newValue, Value& oldValue)
        {
            derived().SetPropertyOverride<Value, Value, false, const std::nullptr_t>(valueField, newValue, oldValue, nullptr_ref);
        }

    #pragma endregion

        template <typename Value, typename OldValue, bool compare, typename PropertyName>
        bool SetPropertyCore(
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
                    RaisePropertyChangedBroadcast(propertyNameOrNames);
                }
            }

            return valueChanged;
        }

        // Single Source - Multiple Slaves
        void RegisterDependency(std::wstring_view source,
            std::initializer_list<const std::wstring_view> dependents)
        {
            auto& vec = m_dependsOnBySource[std::wstring{ source }];
            for (auto d : dependents) vec.emplace_back(d);
        }

        // Single Source - Single Slave
        void RegisterDependency(std::wstring_view source, std::wstring_view dependent)
        {
            m_dependsOnBySource[std::wstring{ source }].emplace_back(dependent);
        }

        void ClearDependencies()
        {
            m_dependsOnBySource = {};
        }

        void ClearDependenciesFrom(std::wstring_view source)
        {
            m_dependsOnBySource.erase(std::wstring{ source });
        }

        template<typename ValidatorUnitT>
        void AddValidator(std::wstring_view property,
            std::function<std::optional<winrt::hstring>(ValidatorUnitT const&)> fn)
        {
            auto& list = m_validators[std::wstring{ property }];
            list.emplace_back([fn = std::move(fn)](winrt::Windows::Foundation::IInspectable const& boxed)
                {
                    return fn(winrt::unbox_value<ValidatorUnitT>(boxed));
                });
        }

        void ClearValidatorsOfProperty(std::wstring_view property)
        {
            m_validators.erase(std::wstring{ property });
        }

        void ClearValidators()
        {
            m_validators = {};
        }

        // 校验值是否正确，并在出错时存储错误信息；函数返回值表示校验结果是否正确
        bool ValidatePropertyValue(std::wstring_view property, winrt::Windows::Foundation::IInspectable const& boxedNewValue)
        {
            if (m_eventValidationRequested)
            {
                winrt::Mvvm::Framework::Core::ValidationRequestedEventArgs req{
                    winrt::hstring{property}, boxedNewValue
                };
                m_eventValidationRequested(derived(), req);
                if (req.Handled())
                {
                    if (req.Cancel()) return false; // 被上层拦截并取消
                    return true;
                }
                if (req.Cancel()) return false;
            }

            std::vector<winrt::hstring> errs;
            if (auto it = m_validators.find(std::wstring{ property }); it != m_validators.end())
            {
                for (auto const& v : it->second)
                {
                    if (auto e = v(boxedNewValue))
                        errs.push_back(*e);
                }
            }

            bool changed = false;
            auto old = GetValidateErrors(property);
            if (errs.empty())
            {
                if (!old.empty())
                {
                    m_validationErrors.erase(std::wstring{ property });
                    changed = true;
                }
            }
            else
            {
                if (old != errs)
                {
                    m_validationErrors[std::wstring{ property }] = errs;
                    changed = true;
                }
            }
            if (changed && m_eventErrorsChanged)
            {
                winrt::Mvvm::Framework::Core::ValidationErrorsChangedEventArgs args
                {
                    winrt::hstring{property},
                    errs
                };
                m_eventErrorsChanged(derived(), args);
            }

            if (m_eventValidationCompleted)
            {
                winrt::Mvvm::Framework::Core::ValidationCompletedEventArgs done
                {
                    winrt::hstring{property},
                    boxedNewValue, errs.empty(),
                    errs
                };
                m_eventValidationCompleted(derived(), done);
            }

            return errs.empty();
        }

        template<typename T>
        bool SetPropertyValidate(T& field, T const& newValue, std::wstring_view propertyName, bool commitOnInvalid = false)
        {
            auto boxed = winrt::box_value(newValue);
            bool ok = ValidatePropertyValue(propertyName, boxed);
            if (!ok && !commitOnInvalid) return false;

            return SetProperty(field, newValue, propertyName);
        }

        bool HasValidateErrors() const { return !m_validationErrors.empty(); }
        bool HasValidateErrors(std::wstring_view property) const
        {
            auto it = m_validationErrors.find(std::wstring{ property });
            return it != m_validationErrors.end() && !it->second.empty();
        }

        std::vector<winrt::hstring> GetValidateErrors(std::wstring_view property) const
        {
            if (auto it = m_validationErrors.find(std::wstring{ property }); it != m_validationErrors.end())
                return it->second;
            return {};
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
        Value GetPropertyOverride(Value const& valueField)
        {
            return derived().GetPropertyCore(valueField);
        }

        template <typename Value, typename OldValue, bool compare, typename PropertyName>
        bool SetPropertyOverride(
            Value& valueField,
            Value const& newValue,
            [[maybe_unused]] OldValue& oldValue,
            [[maybe_unused]] PropertyName const& propertyNameOrNames)
        {
            return SetPropertyCore(valueField, newValue, oldValue, propertyNameOrNames);
        }

        void RaisePropertyChangedEvent(std::wstring_view const& propertyName)
        {
            // Only instantiate the arguments class if the event has any listeners
            if (m_eventPropertyChanged)
            {
                winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs args{ propertyName };
                m_eventPropertyChanged(derived(), args);
            }
        }

        void RaisePropertyChangedEvent(std::initializer_list<const std::wstring_view> const& propertyNames)
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

        void RaisePropertyChangedBroadcast(std::wstring_view const& name)
        {
            if (!m_eventPropertyChanged) return;

            std::unordered_set<std::wstring> visited;
            std::vector<std::wstring> stack{ std::wstring{name} };

            while (!stack.empty())
            {
                std::wstring cur = std::move(stack.back());
                stack.pop_back();
                if (!visited.insert(cur).second) continue;

                winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs args{ cur };
                m_eventPropertyChanged(derived(), args);

                if (auto it = m_dependsOnBySource.find(cur); it != m_dependsOnBySource.end())
                    for (auto const& dep : it->second) stack.push_back(dep);
            }
        }

        // Multi-property broadcasting: when the source of a dependent property changes, 
        // notify all properties that depend on it.
        void RaisePropertyChangedBroadcast(std::initializer_list<const std::wstring_view> const& names)
        {
            for (auto n : names)
                RaisePropertyChangedBroadcast(n);
        }

        private:
            using BoxedValidator = std::function<std::optional<winrt::hstring>(
                winrt::Windows::Foundation::IInspectable const&)>;

            std::unordered_map< std::wstring, std::vector<std::wstring> > m_dependsOnBySource;

            std::unordered_map<std::wstring, std::vector<BoxedValidator>> m_validators;
            std::unordered_map<std::wstring, std::vector<winrt::hstring>> m_validationErrors;

    };
}

#endif // __MVVM_CPPWINRT_NOTIFY_PROPERTY_CHANGED_H_INCLUDED
