export module OpenNet.Core.Setting.LocalSetting;

import winrt.Microsoft.Windows.Storage;
import winrt.Windows.Storage;
import std;
/// <summary>
/// This is a simple settings manager that uses WinRT api for persistence (Serialized Registry Structure).
/// It should not include too many settings, the performance may degrade when there are too many items, and it is not designed for complex data structures.
/// </summary>
export namespace OpenNet::Core::Setting
{
	namespace Storage = winrt::Microsoft::Windows::Storage;

	// WinRT supports types
	// https://learn.microsoft.com/en-us/windows/win32/winrt/base-data-types
	template<typename T>
	concept EnumType =
		std::is_enum_v<T>;

	template<typename T>
	concept LocalSettingType =
		std::same_as<T, bool>

		|| std::same_as<T, std::int16_t>
		|| std::same_as<T, std::int32_t>
		|| std::same_as<T, std::int64_t>

		|| std::same_as<T, std::uint8_t>
		|| std::same_as<T, std::uint16_t>
		|| std::same_as<T, std::uint32_t>
		|| std::same_as<T, std::uint64_t>

		|| std::same_as<T, float>
		|| std::same_as<T, double>

		|| std::same_as<T, char16_t>

		|| std::same_as<T, winrt::guid>
		|| std::same_as<T, winrt::hstring>

		|| std::same_as<T, winrt::Windows::Foundation::DateTime>
		|| std::same_as<T, winrt::Windows::Foundation::TimeSpan>

		|| std::same_as<T, winrt::Windows::Foundation::Point>
		|| std::same_as<T, winrt::Windows::Foundation::Size>
		|| std::same_as<T, winrt::Windows::Foundation::Rect>

		|| std::same_as<
		T,
		winrt::Windows::Storage::ApplicationDataCompositeValue>;

	//================================================
	// LocalSetting
	//================================================

	class LocalSetting final
	{
	public:
		template<LocalSettingType T>
		static T Get(winrt::hstring const& key, T const& defaultValue = {})
		{
			auto values = Values();

			if (auto object = values.TryLookup(key))
			{
				return winrt::unbox_value<T>(object);
			}

			Set(key, defaultValue);

			return defaultValue;
		}

		template<EnumType T>
		static T Get(winrt::hstring const& key, T defaultValue = {})
		{
			using underlying = std::underlying_type_t<T>;

			return static_cast<T>(
				Get<underlying>(
					key,
					static_cast<underlying>(defaultValue)));
		}

		template<LocalSettingType T>
		static void Set(winrt::hstring const& key, T const& value)
		{
			Values().Insert(
				key,
				winrt::box_value(value));
		}

		template<EnumType T>
		static void Set(winrt::hstring const& key, T value)
		{
			using underlying = std::underlying_type_t<T>;

			Set(key, static_cast<underlying>(value));
		}

		template<LocalSettingType T>
		static T Update(winrt::hstring const& key, T const& defaultValue, std::invocable<T> auto modifier)
		{
			T oldValue = Get(key, defaultValue);
			T newValue = modifier(oldValue);
			Set(key, newValue);

			return oldValue;
		}

		template<EnumType T>
		static T Update(winrt::hstring const& key, T defaultValue, std::invocable<T> auto modifier)
		{
			T oldValue = Get(key, defaultValue);

			T newValue = modifier(oldValue);

			Set(key, newValue);

			return oldValue;
		}

		template<typename T>
		static void SetIf(bool condition, winrt::hstring const& key, T const& value)
		{
			if (condition)
			{
				Set(key, value);
			}
		}

		template<typename T>
		static void SetIfNot(bool condition, winrt::hstring const& key, T const& value)
		{
			if (!condition)
			{
				Set(key, value);
			}
		}

	private:

		static auto Values()
		{
			static auto values =
				Storage::ApplicationData::
				GetDefault().
				LocalSettings().
				Values();

			return values;
		}
	};
}