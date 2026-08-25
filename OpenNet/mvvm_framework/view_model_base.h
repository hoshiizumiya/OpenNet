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

#include "notify_property_changed.h"

import winrt.Windows.Foundation;
import winrt.Microsoft.UI.Dispatching;

namespace mvvm
{
	template <typename Derived>
	struct __declspec(empty_bases)ViewModelBase : WrapNotifyPropertyChanged<Derived>
	{
		friend typename Derived;

		template <typename TValue>
		inline TValue GetPropertyOverride(TValue const& valueField)
		{
			std::scoped_lock const guard{ m_propertyMutex };
			return base::notify_property_changed::GetPropertyCore(valueField);
		}

		template <typename TValue, typename TOldValue, bool compare, typename propertyNameType>
		inline bool SetPropertyOverride(TValue& valueField, TValue const& newValue, TOldValue& oldValue, propertyNameType const& propertyNameOrNames)
		{
			constexpr bool hasOldValue = !std::is_null_pointer_v<TOldValue>;
			bool valueChanged{};
			{
				std::scoped_lock const guard{ m_propertyMutex };
				if constexpr (hasOldValue) oldValue = valueField;
				if constexpr (compare)
				{
					valueChanged = valueField != newValue;
					if (valueChanged) valueField = newValue;
				}
				else
				{
					valueField = newValue;
					valueChanged = true;
				}
			}
			if (!valueChanged) return false;

			using PropertyNames = std::remove_cvref_t<propertyNameType>;
			if constexpr (!std::is_same_v<PropertyNames, std::nullptr_t>)
			{
				std::vector<std::wstring> propertyNames;
				if constexpr (std::is_convertible_v<propertyNameType, std::wstring_view>)
				{
					propertyNames.emplace_back(std::wstring_view{ propertyNameOrNames });
				}
				else
				{
					for (auto const propertyName : propertyNameOrNames)
					{
						propertyNames.emplace_back(propertyName);
					}
				}
				auto notify = [weak = this->derived().get_weak(), propertyNames = std::move(propertyNames)]
				{
					if (auto self = weak.get())
					{
						for (auto const& propertyName : propertyNames)
						{
							self->RaisePropertyChangedBroadcast(propertyName);
						}
					}
				};
				if (this->derived().HasThreadAccess())
				{
					notify();
				}
				else if (auto dispatcher = this->derived().GetDispatcherOverride())
				{
					dispatcher.TryEnqueue(std::move(notify));
				}
			}
			return true;
		}

		winrt::Microsoft::UI::Dispatching::DispatcherQueue GetDispatcherOverride()
		{
			return { nullptr };
		}

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
		mutable std::mutex m_propertyMutex;

		// This is used to ensure that the derived class is actually derived from ViewModelBase
		ViewModelBase()
		{
			static_assert(std::is_base_of_v<ViewModelBase, Derived>, "Derived class must inherit from ViewModelBase");
		}
		using base = typename ::mvvm::WrapNotifyPropertyChanged<Derived>;
	};
}

#endif // __MVVM_CPPWINRT_VIEW_MODEL_BASE_H_INCLUDED

import winrt.Windows.Foundation;
import winrt.Microsoft.UI.Dispatching;
