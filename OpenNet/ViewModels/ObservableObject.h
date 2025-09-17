#pragma once
#include "ViewModels/ObservableObject.g.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>

namespace winrt::OpenNet::ViewModels::implementation
{
	struct ObservableObject : ObservableObjectT<ObservableObject>
	{
		ObservableObject() = default;

		// 可观察对象基类 / Observable Object Base Class
		// INotifyPropertyChanged 实现 / INotifyPropertyChanged Implementation
		winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
		{
			return m_propertyChanged.add(handler);
		}

		void PropertyChanged(winrt::event_token const& token) noexcept
		{
			m_propertyChanged.remove(token);
		}

	protected:
		// 属性设置助手 / Property Setter Helper
		template<typename T>
		bool SetProperty(T& storage, T const& value, winrt::hstring const& propertyName)
		{
			if (storage != value)
			{
				storage = value;
				RaisePropertyChanged(propertyName);
				return true;
			}
			return false;
		}

		// 引发属性变更事件 / Raise Property Changed Event
		void RaisePropertyChanged(winrt::hstring const& propertyName)
		{
			m_propertyChanged(*this, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
		}

	private:
		winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
	};

	// 可观察集合助手 / Observable Collection Helper
	template<typename T>
	struct ObservableCollection
	{
		static winrt::Windows::Foundation::Collections::IObservableVector<T> Create()
		{
			return winrt::single_threaded_observable_vector<T>();
		}

		static winrt::Windows::Foundation::Collections::IObservableVector<T> Create(winrt::Windows::Foundation::Collections::IIterable<T> const& items)
		{
			auto collection = winrt::single_threaded_observable_vector<T>();
			for (auto const& item : items)
			{
				collection.Append(item);
			}
			return collection;
		}
	};

	// 属性验证助手 / Property Validation Helper
	struct ValidationHelper
	{
		struct ValidationResult
		{
			bool isValid;
			winrt::hstring errorMessage;

			ValidationResult(bool valid = true, winrt::hstring const& message = L"")
				: isValid(valid), errorMessage(message)
			{
			}
		};

		// 验证非空字符串 / Validate Non-Empty String
		static ValidationResult ValidateNotEmpty(winrt::hstring const& value, winrt::hstring const& fieldName)
		{
			if (value.empty())
				return { false, fieldName + L" 不能为空 / cannot be empty" };
			return { true };
		}

		// 验证数字范围 / Validate Number Range
		template<typename T>
		static ValidationResult ValidateRange(T value, T minValue, T maxValue, winrt::hstring const& fieldName)
		{
			if (value < minValue || value > maxValue)
			{
				return { false, fieldName + L" 必须在 " + winrt::to_hstring(minValue) +
					 L" 到 " + winrt::to_hstring(maxValue) + L" 之间 / must be between " +
					 winrt::to_hstring(minValue) + L" and " + winrt::to_hstring(maxValue) };
			}
			return { true };
		}

		// 验证端口号 / Validate Port Number
		static ValidationResult ValidatePort(uint16_t port)
		{
			return ValidateRange<uint16_t>(port, 1, 65535, L"端口号 / Port number");
		}

		// 验证IP地址格式 / Validate IP Address Format
		static ValidationResult ValidateIPAddress(winrt::hstring const& ipAddress)
		{
			if (ipAddress.empty())
				return { false, L"IP地址不能为空 / IP address cannot be empty" };

			// 简化的IP地址验证
			bool hasValidChars = true;
			for (auto c : ipAddress)
			{
				if (!(c >= L'0' && c <= L'9') && c != L'.' && c != L':' &&
					!(c >= L'a' && c <= L'f') && !(c >= L'A' && c <= L'F'))
				{
					hasValidChars = false;
					break;
				}
			}

			if (!hasValidChars)
				return { false, L"无效的IP地址格式 / Invalid IP address format" };

			return { true };
		}
	};
}

// 为原生视图模型提供别名，便于继承和调用助手方法
// Alias for native ViewModels to derive from and use helper methods
namespace OpenNet::ViewModels
{
	using ObservableObject = winrt::OpenNet::ViewModels::implementation::ObservableObject;
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct ObservableObject : ObservableObjectT<ObservableObject, implementation::ObservableObject>
	{
	};
}
