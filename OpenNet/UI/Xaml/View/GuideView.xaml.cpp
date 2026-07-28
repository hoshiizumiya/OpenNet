#include "XamlWorkaround.h"
#include "GuideView.xaml.h"
#if __has_include("UI/Xaml/View/GuideView.g.cpp")
#include "UI/Xaml/View/GuideView.g.cpp"
#endif

import winrt.Windows.Globalization;
import winrt.Microsoft.Windows.Globalization;
import winrt.Microsoft.Windows.AppLifecycle;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.ViewModels.Guide.GuideState;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	GuideView::GuideView()
	{
		this->ViewModel(winrt::make<winrt::OpenNet::ViewModels::Guide::implementation::GuideViewModel>());
		m_cultures = single_threaded_observable_vector<winrt::OpenNet::Models::NameCultureInfoValue>();

		auto const current = winrt::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride();
		auto const languages = winrt::Microsoft::Windows::Globalization::ApplicationLanguages::ManifestLanguages();
		for (auto const& tag : languages)
		{
			winrt::Windows::Globalization::Language language{ tag };
			auto item = winrt::make<winrt::OpenNet::Models::implementation::NameCultureInfoValue>();
			item.Name(language.NativeName());
			item.Value(tag);
			item.IsMaintainedByMSTRDI(tag == L"zh-CN" || tag == L"zh-Hant");
			item.IsMaintainedByCrowdin(!item.IsMaintainedByMSTRDI());
			m_cultures.Append(item);

			if (!m_allCulturesWelcomeText.empty()) m_allCulturesWelcomeText = m_allCulturesWelcomeText + L"+";
			m_allCulturesWelcomeText = m_allCulturesWelcomeText + language.NativeName();
			if ((!current.empty() && current == tag) || (current.empty() && !this->ViewModel().SelectedCulture()))
			{
				this->ViewModel().SelectedCulture(item);
			}
		}

		if (!this->ViewModel().SelectedCulture() && m_cultures.Size() > 0) this->ViewModel().SelectedCulture(m_cultures.GetAt(0));
	}

	void GuideView::InitializeComponent()
	{
		GuideViewT::InitializeComponent();
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		WebUIAddressBox().Text(to_hstring(
			database.GetString("webui_host", "address")
			.value_or("127.0.0.1")));
		WebUIPortBox().Value(static_cast<double>(
			database.GetInt("webui_host", "port").value_or(8080)));
		WebUIUserNameBox().Text(to_hstring(
			database.GetString("webui_host", "username")
			.value_or("admin")));
		WebUIPasswordBox().Password(to_hstring(
			database.GetString("webui_host", "password")
			.value_or("")));
		GuiRefreshIntervalBox().Value(static_cast<double>(
			std::clamp<std::int64_t>(
				database.GetInt("ui", "refresh_interval_ms")
				.value_or(1000),
				100,
				60000)));
	}

	OpenNet::ViewModels::Guide::GuideViewModel GuideView::ViewModel()
	{
		return GuideViewMvvmBase::ViewModel();
	}
	void GuideView::ViewModel(OpenNet::ViewModels::Guide::GuideViewModel const& value)
	{
		GuideViewMvvmBase::ViewModel(value);
	}
	std::int32_t GuideView::StateIndex()
	{
		return static_cast<std::int32_t>(this->ViewModel().State());
	}
	std::int32_t GuideView::StateIndexFromState(std::uint32_t value)
	{
		return static_cast<std::int32_t>(value);
	}

	hstring GuideView::AllCulturesWelcomeText()
	{
		return m_allCulturesWelcomeText;
	}
	Windows::Foundation::Collections::IObservableVector<OpenNet::Models::NameCultureInfoValue> GuideView::Cultures()
	{
		return m_cultures;
	}
	OpenNet::Models::NameCultureInfoValue GuideView::SelectedCulture()
	{
		return this->ViewModel().SelectedCulture();
	}
	void GuideView::SelectedCulture(OpenNet::Models::NameCultureInfoValue const& value)
	{
		auto const previous = this->ViewModel().SelectedCulture();
		if (!value || (previous && previous.Value() == value.Value())) return;
		this->ViewModel().SelectedCulture(value);
		if (previous) winrt::Microsoft::Windows::AppLifecycle::AppInstance::Restart(L"");
	}

	bool GuideView::AgreementUseGrid()
	{
		auto languages = Windows::Globalization::ApplicationLanguages::Languages();
		if (languages.Size() == 0) return false;
		auto const tag = languages.GetAt(0);
		return tag.size() >= 2 && (tag[0] == L'z' || tag[0] == L'Z') && (tag[1] == L'h' || tag[1] == L'H');
	}

	hstring GuideView::AgreementCopyTarget()
	{
		return AgreementUseGrid()
			? L"我已阅读、理解并同意上述条款。"
			: L"I have read, understood, and agree to the terms above.";
	}
	bool GuideView::IsTermOfServiceAgreed()
	{
		return this->ViewModel().IsTermOfServiceAgreed();
	}
	void GuideView::IsTermOfServiceAgreed(bool value)
	{
		this->ViewModel().IsTermOfServiceAgreed(value);
	}
	bool GuideView::IsPrivacyPolicyAgreed()
	{
		return this->ViewModel().IsPrivacyPolicyAgreed();
	}
	void GuideView::IsPrivacyPolicyAgreed(bool value)
	{
		this->ViewModel().IsPrivacyPolicyAgreed(value);
	}
	bool GuideView::IsIssueReportAgreed()
	{
		return this->ViewModel().IsIssueReportAgreed();
	}
	void GuideView::IsIssueReportAgreed(bool value)
	{
		this->ViewModel().IsIssueReportAgreed(value);
	}
	bool GuideView::IsOpenSourceLicenseAgreed()
	{
		return this->ViewModel().IsOpenSourceLicenseAgreed();
	}
	void GuideView::IsOpenSourceLicenseAgreed(bool value)
	{
		this->ViewModel().IsOpenSourceLicenseAgreed(value);
	}
	bool GuideView::IsAgreementCopyAgreed()
	{
		return this->ViewModel().IsAgreementCopyAgreed();
	}
	void GuideView::IsAgreementCopyAgreed(bool value)
	{
		this->ViewModel().IsAgreementCopyAgreed(value);
	}
	void GuideView::NextOrComplete(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		auto const state = this->ViewModel().State();
		if (state == static_cast<std::uint32_t>(
			::OpenNet::ViewModels::Guide::GuideState::WebUI)
			&& !SaveWebUISettings())
		{
			return;
		}
		if (state == static_cast<std::uint32_t>(
			::OpenNet::ViewModels::Guide::GuideState::CommonSetting))
		{
			SaveGuiSettings();
		}
		if (state == static_cast<std::uint32_t>(
			::OpenNet::ViewModels::Guide::GuideState::Completed))
		{
			auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
			database.Initialize();
			database.SetBool("webui_host", "initialized", true);
			m_completed(*this, nullptr);
			return;
		}
		this->ViewModel().State(state + 1);
	}

	bool GuideView::SaveWebUISettings()
	{
		auto const address = to_string(WebUIAddressBox().Text());
		auto const username = to_string(WebUIUserNameBox().Text());
		auto const password = to_string(WebUIPasswordBox().Password());
		auto const port = static_cast<std::int64_t>(WebUIPortBox().Value());
		const bool valid = !address.empty()
			&& !username.empty()
			&& password.size() >= 6
			&& port > 0
			&& port <= 65535;
		WebUIValidationInfoBar().IsOpen(!valid);
		if (!valid)
		{
			return false;
		}

		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		database.SetString("webui_host", "address", address);
		database.SetInt("webui_host", "port", port);
		database.SetString("webui_host", "username", username);
		database.SetString("webui_host", "password", password);
		return true;
	}

	void GuideView::SaveGuiSettings()
	{
		auto value = static_cast<std::int64_t>(
			std::isnan(GuiRefreshIntervalBox().Value())
			? 1000
			: GuiRefreshIntervalBox().Value());
		value = std::clamp<std::int64_t>(value, 100, 60000);
		GuiRefreshIntervalBox().Value(static_cast<double>(value));
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		database.SetInt("ui", "refresh_interval_ms", value);
	}

	event_token GuideView::Completed(
		Windows::Foundation::EventHandler<IInspectable> const& handler)
	{
		return m_completed.add(handler);
	}

	void GuideView::Completed(event_token const& token) noexcept
	{
		m_completed.remove(token);
	}
}
