#include "WindowsPlatform.h"
#include "XamlWorkaround.h"
#include "LiquidGlassProfile.h"
#include "MaterialControlState.h"
#include "ControlAppearance.h"
#if __has_include("UI/Xaml/Control/Theme/LiquidGlassProfile.g.cpp")
#include "UI/Xaml/Control/Theme/LiquidGlassProfile.g.cpp"
#endif
#if __has_include("UI/Xaml/Control/Theme/LiquidGlassParameter.g.cpp")
#include "UI/Xaml/Control/Theme/LiquidGlassParameter.g.cpp"
#endif
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Utils.Message;
import OpenNet.XamlApplicationLifetime;

using namespace winrt;
using namespace Microsoft::UI::Xaml;
namespace optics = ::OpenNet::UI::LiquidGlassOptics;

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
	namespace
	{
		constexpr char Category[] = "liquid_glass_profiles_v1";
		Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile> s_profiles{ nullptr };
		std::string StorageKey(hstring const& kind, hstring const& parameter)
		{
			return to_string(kind) + "." + to_string(parameter);
		}
		void ReportRejected(hstring const& kind, hstring const& parameter, double value, hstring const& message) noexcept
		{
			try
			{
				auto text = std::format(L"LiquidGlass: profile={}, parameter={}, rejected={:.17g}, HRESULT=0x80070057: {}\n",
										std::wstring_view(kind), std::wstring_view(parameter), value, std::wstring_view(message));
				OutputDebugStringW(text.c_str());
			}
			catch (...)
			{
				OutputDebugStringW(L"LiquidGlass: rejected parameter; diagnostic formatting failed.\n");
			}
		}
		bool IsEnabled(hstring const& kind)
		{
			auto state = MaterialControlState::Current();
			if (kind == L"Button") return state.ButtonEnabled();
			if (kind == L"ToggleButton") return state.ToggleButtonEnabled();
			if (kind == L"HyperlinkButton") return state.HyperlinkButtonEnabled();
			if (kind == L"CheckBox") return state.CheckBoxEnabled();
			if (kind == L"RadioButton") return state.RadioButtonEnabled();
			if (kind == L"Slider") return state.SliderEnabled();
			if (kind == L"TextBox") return state.TextBoxEnabled();
			if (kind == L"SearchBox") return state.SearchBoxEnabled();
			if (kind == L"PasswordBox") return state.PasswordBoxEnabled();
			if (kind == L"ComboBox") return state.ComboBoxEnabled();
			if (kind == L"ToggleSwitch") return state.ToggleSwitchEnabled();
			if (kind == L"Card") return state.CardEnabled();
			if (kind == L"FloatingPanel") return state.FloatingPanelEnabled();
			if (kind == L"TabBar") return state.TabBarEnabled();
			return false;
		}
		void SetEnabled(hstring const& kind, bool value)
		{
			auto state = MaterialControlState::Current();
			if (kind == L"Button") state.ButtonEnabled(value);
			if (kind == L"ToggleButton") state.ToggleButtonEnabled(value);
			if (kind == L"HyperlinkButton") state.HyperlinkButtonEnabled(value);
			if (kind == L"CheckBox") state.CheckBoxEnabled(value);
			if (kind == L"RadioButton") state.RadioButtonEnabled(value);
			if (kind == L"Slider") state.SliderEnabled(value);
			if (kind == L"TextBox") state.TextBoxEnabled(value);
			if (kind == L"SearchBox") state.SearchBoxEnabled(value);
			if (kind == L"PasswordBox") state.PasswordBoxEnabled(value);
			if (kind == L"ComboBox") state.ComboBoxEnabled(value);
			if (kind == L"ToggleSwitch") state.ToggleSwitchEnabled(value);
			if (kind == L"Card") state.CardEnabled(value);
			if (kind == L"FloatingPanel") state.FloatingPanelEnabled(value);
			if (kind == L"TabBar") state.TabBarEnabled(value);
		}
	}

	LiquidGlassParameter::LiquidGlassParameter(hstring const& kind, std::size_t index, double value)
		: m_kind(kind), m_index(index), m_default(value),
		m_state(std::min(optics::Descriptors[index].Minimum, value), std::max(optics::Descriptors[index].Maximum, value), value)
	{
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto stored = db.GetDouble(Category, StorageKey(kind, Key()));
		if (stored && !m_state.Load(*stored))
		{
			Reject(ResourceGetString(L"GlassParameterInvalidValue"));
			ReportRejected(kind, Key(), *stored, ErrorMessage());
			db.Delete(Category, StorageKey(kind, Key()));
		}
		else if (stored && !m_state.Overridden()) db.Delete(Category, StorageKey(kind, Key()));
		SetValue(ValueProperty(), box_value(m_state.Value()));
		m_loading = false;
	}
	hstring LiquidGlassParameter::Key() const
	{
		return optics::Descriptors[m_index].Key;
	}
	hstring LiquidGlassParameter::Label() const
	{
		return ResourceGetString((L"GlassParameter_" + Key()).c_str());
	}
	double LiquidGlassParameter::Minimum() const
	{
		return std::min(optics::Descriptors[m_index].Minimum, m_default);
	}
	double LiquidGlassParameter::Maximum() const
	{
		return std::max(optics::Descriptors[m_index].Maximum, m_default);
	}
	double LiquidGlassParameter::Step() const
	{
		return optics::Descriptors[m_index].Step;
	}
	DependencyProperty LiquidGlassParameter::ValueProperty()
	{
		static auto property = DependencyProperty::Register(L"Value", xaml_typename<double>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ Changed } });
		return property;
	}
	double LiquidGlassParameter::Value() const
	{
		return m_state.Value();
	}
	void LiquidGlassParameter::Value(double value)
	{
		if (!m_state.Accepts(value))
		{
			Reject(ResourceGetString(L"GlassParameterInvalidValue"));
			ReportRejected(m_kind, Key(), value, ErrorMessage());
			SynchronizeValue();
			return;
		}
		SetValue(ValueProperty(), box_value(value));
	}
	void LiquidGlassParameter::Changed(DependencyObject const& sender, DependencyPropertyChangedEventArgs const& args)
	{
		auto self = get_self<LiquidGlassParameter>(sender.as<class_type>());
		if (self->m_loading) return;
		try
		{
			auto value = unbox_value<double>(args.NewValue());
			auto previous = self->Value();
			// SetValue/TwoWay bindings can bypass the C++ Value(double) wrapper.
			if (!self->m_state.Stage(value))
			{
				self->Reject(ResourceGetString(L"GlassParameterInvalidValue"));
				ReportRejected(self->m_kind, self->Key(), value, self->ErrorMessage());
				self->SynchronizeValue();
				return;
			}
			self->Reject({});
			if (previous != value) MaterialControlState::RefreshBrushes();
		}
		catch (...)
		{
			LiquidGlassProfile::ReportCurrentFailure(L"Parameter change callback");
			try
			{
				self->Rollback();
			}
			catch (...)
			{
				LiquidGlassProfile::ReportCurrentFailure(L"Parameter callback rollback");
			}
		}
	}
	void LiquidGlassParameter::SynchronizeValue()
	{
		if (::OpenNet::XamlApplicationLifetime::Exiting.load()) return;
		m_loading = true;
		try
		{
			SetValue(ValueProperty(), box_value(m_state.Value()));
		}
		catch (...)
		{
			m_loading = false; throw;
		}
		m_loading = false;
	}
	DependencyProperty LiquidGlassParameter::ErrorMessageProperty()
	{
		static auto property = DependencyProperty::Register(L"ErrorMessage", xaml_typename<hstring>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(L"") });
		return property;
	}
	DependencyProperty LiquidGlassParameter::HasErrorProperty()
	{
		static auto property = DependencyProperty::Register(L"HasError", xaml_typename<bool>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(false) });
		return property;
	}
	hstring LiquidGlassParameter::ErrorMessage() const
	{
		return unbox_value<hstring>(GetValue(ErrorMessageProperty()));
	}
	bool LiquidGlassParameter::HasError() const
	{
		return unbox_value<bool>(GetValue(HasErrorProperty()));
	}
	void LiquidGlassParameter::Reject(hstring const& message)
	{
		if (::OpenNet::XamlApplicationLifetime::Exiting.load()) return;
		SetValue(ErrorMessageProperty(), box_value(message));
		SetValue(HasErrorProperty(), box_value(!message.empty()));
	}
	void LiquidGlassParameter::Commit()
	{
		if (!m_state.Pending()) return;
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto key = StorageKey(m_kind, Key());
		if (Overridden()) db.SetDouble(Category, key, Value());
		else db.Delete(Category, key);
		m_state.Commit();
	}
	void LiquidGlassParameter::Rollback()
	{
		if (!m_state.Pending()) return;
		m_state.Rollback();
		SynchronizeValue();
	}
	bool LiquidGlassParameter::RestoreRejected()
	{
		auto committed = m_state.Committed();
		if (!m_state.RestoreRejected()) return false;
		if (committed != m_state.Committed())
			::OpenNet::Core::AppSettingsDatabase::Instance().Delete(Category, StorageKey(m_kind, Key()));
		SynchronizeValue();
		return true;
	}
	void LiquidGlassParameter::Reset()
	{
		m_state.Reset();
		Reject({});
		SynchronizeValue();
		MaterialControlState::RefreshBrushes();
	}

	LiquidGlassProfile::LiquidGlassProfile(hstring const& key, hstring const&) : m_key(key), m_name(ResourceGetString((L"GlassKind_" + key).c_str()))
	{
		m_parameters = single_threaded_observable_vector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassParameter>();
		auto defaults = optics::Defaults(key);
		for (std::size_t i = 0; i < optics::Descriptors.size(); ++i)
			m_parameters.Append(make<LiquidGlassParameter>(key, i, optics::Descriptors[i].Read(defaults)));
		Enabled(IsEnabled(key));
		m_loading = false;
		// Validate the saved baseline through real native setters before first use.
		ApplyToBrush(defaults, optics::Capture(defaults));
	}
	Windows::Foundation::Collections::IObservableVector<LiquidGlassProfile::class_type> LiquidGlassProfile::All()
	{
		if (!s_profiles)
		{
			auto profiles = single_threaded_observable_vector<class_type>();
			profiles.Append(make<LiquidGlassProfile>(L"Button", L"Buttons"));
			profiles.Append(make<LiquidGlassProfile>(L"ToggleButton", L"Toggle buttons"));
			profiles.Append(make<LiquidGlassProfile>(L"HyperlinkButton", L"Hyperlinks"));
			profiles.Append(make<LiquidGlassProfile>(L"CheckBox", L"Check boxes"));
			profiles.Append(make<LiquidGlassProfile>(L"RadioButton", L"Radio buttons"));
			profiles.Append(make<LiquidGlassProfile>(L"Slider", L"Sliders"));
			profiles.Append(make<LiquidGlassProfile>(L"TextBox", L"Text inputs"));
			profiles.Append(make<LiquidGlassProfile>(L"SearchBox", L"Search inputs"));
			profiles.Append(make<LiquidGlassProfile>(L"PasswordBox", L"Password inputs"));
			profiles.Append(make<LiquidGlassProfile>(L"ComboBox", L"Combo boxes"));
			profiles.Append(make<LiquidGlassProfile>(L"ToggleSwitch", L"Switches"));
			profiles.Append(make<LiquidGlassProfile>(L"Card", L"Cards and surfaces"));
			profiles.Append(make<LiquidGlassProfile>(L"FloatingPanel", L"Floating panels"));
			profiles.Append(make<LiquidGlassProfile>(L"TabBar", L"Tab bars"));
			s_profiles = profiles;
		}
		return s_profiles;
	}
	DependencyProperty LiquidGlassProfile::EnabledProperty()
	{
		static auto property = DependencyProperty::Register(L"Enabled", xaml_typename<bool>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(true), PropertyChangedCallback{ Changed } });
		return property;
	}
	bool LiquidGlassProfile::Enabled() const
	{
		return unbox_value<bool>(GetValue(EnabledProperty()));
	}
	void LiquidGlassProfile::Enabled(bool value)
	{
		SetValue(EnabledProperty(), box_value(value));
	}
	void LiquidGlassProfile::Changed(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
	{
		auto self = get_self<LiquidGlassProfile>(sender.as<class_type>());
		if (!self->m_loading) SetEnabled(self->Key(), self->Enabled());
	}
	void LiquidGlassProfile::SyncEnabled()
	{
		if (!s_profiles) return;
		for (auto const& profile : s_profiles)
		{
			auto self = get_self<LiquidGlassProfile>(profile);
			self->m_loading = true;
			profile.Enabled(IsEnabled(profile.Key()));
			self->m_loading = false;
		}
	}
	void LiquidGlassProfile::Reset()
	{
		// Reset only this type's optics. Its enable switch and other types are untouched.
		for (auto const& parameter : m_parameters) parameter.Reset();
	}
	LiquidGlassApplyResult LiquidGlassProfile::Apply(hstring const& kind, WinUI::Composition::Hlsl::LiquidGlassBrush const& brush, optics::Snapshot const& defaults, bool overridesOnly)
	{
		auto key = optics::Canonical(kind);
		for (auto const& profile : All())
		{
			if (profile.Key() != key) continue;
			return get_self<LiquidGlassProfile>(profile)->ApplyToBrush(brush, defaults, overridesOnly);
		}
		return LiquidGlassApplyResult::Unsupported;
	}
	LiquidGlassApplyResult LiquidGlassProfile::ApplyToBrush(WinUI::Composition::Hlsl::LiquidGlassBrush const& brush, optics::Snapshot const& defaults, bool overridesOnly)
	{
		auto result = LiquidGlassApplyResult::Applied;
		for (std::uint32_t i = 0; i < m_parameters.Size(); ++i)
		{
			auto parameter = get_self<LiquidGlassParameter>(m_parameters.GetAt(i));
			if (overridesOnly && !parameter->Overridden()) continue;
			auto parameterResult = ::OpenNet::UI::ApplyLiquidGlassParameter(*parameter, defaults[i], [&](double effective)
			{
				try
				{
					optics::Descriptors[i].Write(brush, effective);
					return true;
				}
				catch (hresult_error const& error)
				{
					if (error.code() != E_INVALIDARG) throw;
					parameter->Reject(error.message());
					ReportRejected(m_key, parameter->Key(), effective, error.message());
					return false;
				}
			});
			result = std::max(result, parameterResult);
		}
		if (result == LiquidGlassApplyResult::Unsupported) brush.IsEnabled(false);
		return result;
	}
	void LiquidGlassProfile::ValidatePendingChanges()
	{
		for (auto const& profile : All())
		{
			auto self = get_self<LiquidGlassProfile>(profile);
			bool pending = false;
			for (auto const& parameter : self->m_parameters)
				pending = pending || get_self<LiquidGlassParameter>(parameter)->Pending();
			if (!pending) continue;
			auto probe = optics::Defaults(profile.Key());
			if (self->ApplyToBrush(probe, optics::Capture(probe)) == LiquidGlassApplyResult::Unsupported)
				throw hresult_error(E_INVALIDARG, L"Liquid glass library rejected its preset.");
		}
	}
	void LiquidGlassProfile::CommitChanges()
	{
		for (auto const& profile : All())
			for (auto const& parameter : profile.Parameters()) get_self<LiquidGlassParameter>(parameter)->Commit();
	}
	void LiquidGlassProfile::RollbackChanges() noexcept
	{
		if (!s_profiles) return;
		try
		{
			for (auto const& profile : s_profiles)
				for (auto const& parameter : profile.Parameters())
				{
					try
					{
						get_self<LiquidGlassParameter>(parameter)->Rollback();
					}
					catch (...)
					{
						ReportCurrentFailure(L"Rollback parameter");
					}
				}
		}
		catch (...)
		{
			ReportCurrentFailure(L"Rollback profiles");
		}
	}
	unsigned long long LiquidGlassProfile::Revision()
	{
		unsigned long long revision = 0;
		for (auto const& profile : All())
			for (auto const& parameter : profile.Parameters()) revision += get_self<LiquidGlassParameter>(parameter)->Revision();
		return revision;
	}
	void LiquidGlassProfile::ReportCurrentFailure(wchar_t const* operation) noexcept
	{
		try
		{
			try
			{
				throw;
			}
			catch (hresult_error const& error)
			{
				auto text = std::format(L"LiquidGlass: {}; HRESULT=0x{:08X}: {}\n", operation,
										static_cast<std::uint32_t>(error.code()), std::wstring_view(error.message()));
				OutputDebugStringW(text.c_str());
			}
			catch (std::exception const& error)
			{
				OutputDebugStringW(operation); OutputDebugStringA(error.what()); OutputDebugStringW(L"\n");
			}
			catch (...)
			{
				OutputDebugStringW(operation); OutputDebugStringW(L": unknown liquid glass failure.\n");
			}
		}
		catch (...)
		{
			OutputDebugStringW(L"LiquidGlass: diagnostic reporting failed.\n");
		}
	}
}
