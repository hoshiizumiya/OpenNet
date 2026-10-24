#pragma once
#include "UI/Xaml/Control/Theme/LiquidGlassParameter.g.h"
#include "UI/Xaml/Control/Theme/LiquidGlassProfile.g.h"
#include "LiquidGlassOptics.h"
#include "LiquidGlassParameterState.h"

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
	using LiquidGlassApplyResult = ::OpenNet::UI::LiquidGlassParameterApplyResult;
	struct LiquidGlassParameter : LiquidGlassParameterT<LiquidGlassParameter>
	{
		LiquidGlassParameter() = default;
		LiquidGlassParameter(hstring const& kind, std::size_t index, double value);
		hstring Key() const;
		hstring Label() const;
		double Minimum() const;
		double Maximum() const;
		double Step() const;
		double DefaultValue() const
		{
			return m_default;
		}
		double Value() const;
		void Value(double value);
		static Microsoft::UI::Xaml::DependencyProperty ValueProperty();
		void Reset();
		bool Overridden() const
		{
			return m_state.Overridden();
		}
		bool Pending() const
		{
			return m_state.Pending();
		}
		unsigned long long Revision() const
		{
			return m_state.Revision();
		}
		bool RestoreRejected();
		void Commit();
		void Rollback();
		void Reject(winrt::hstring const& message);
		winrt::hstring ErrorMessage() const;
		bool HasError() const;
		static winrt::Microsoft::UI::Xaml::DependencyProperty ErrorMessageProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty HasErrorProperty();
	private:
		void SynchronizeValue();
		static void Changed(Microsoft::UI::Xaml::DependencyObject const&, Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		hstring m_kind;
		std::size_t m_index{};
		double m_default{};
		::OpenNet::UI::LiquidGlassParameterState m_state;
		bool m_loading{ true };
	};

	struct LiquidGlassProfile : LiquidGlassProfileT<LiquidGlassProfile>
	{
		LiquidGlassProfile() = default;
		LiquidGlassProfile(hstring const& key, hstring const& name);
		static Windows::Foundation::Collections::IObservableVector<class_type> All();
		hstring Key() const
		{
			return m_key;
		}
		hstring Name() const
		{
			return m_name;
		}
		bool Enabled() const;
		void Enabled(bool value);
		static Microsoft::UI::Xaml::DependencyProperty EnabledProperty();
		Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassParameter> Parameters() const
		{
			return m_parameters;
		}
		void Reset();
		static void SyncEnabled();
		static LiquidGlassApplyResult Apply(hstring const& kind, WinUI::Composition::Hlsl::LiquidGlassBrush const& brush,
											::OpenNet::UI::LiquidGlassOptics::Snapshot const& defaults, bool overridesOnly = false);
		static void ValidatePendingChanges();
		static void CommitChanges();
		static void RollbackChanges() noexcept;
		static unsigned long long Revision();
		static void ReportCurrentFailure(wchar_t const* operation) noexcept;
	private:
		LiquidGlassApplyResult ApplyToBrush(WinUI::Composition::Hlsl::LiquidGlassBrush const& brush,
											::OpenNet::UI::LiquidGlassOptics::Snapshot const& defaults, bool overridesOnly = false);
		static void Changed(Microsoft::UI::Xaml::DependencyObject const&, Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		hstring m_key, m_name;
		bool m_loading{ true };
		Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassParameter> m_parameters{ nullptr };
	};
}
namespace winrt::OpenNet::UI::Xaml::Control::Theme::factory_implementation
{
	struct LiquidGlassParameter : LiquidGlassParameterT<LiquidGlassParameter, implementation::LiquidGlassParameter>
	{
	};
	struct LiquidGlassProfile : LiquidGlassProfileT<LiquidGlassProfile, implementation::LiquidGlassProfile>
	{
	};
}
