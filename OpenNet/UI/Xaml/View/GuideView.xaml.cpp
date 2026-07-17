#include "XamlWorkaround.h"
#include "GuideView.xaml.h"
#if __has_include("UI/Xaml/View/GuideView.g.cpp")
#include "UI/Xaml/View/GuideView.g.cpp"
#endif

import winrt.Windows.Globalization;
import winrt.Microsoft.Windows.Globalization;
import winrt.Microsoft.Windows.AppLifecycle;

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
		InitializeComponent();
	}

	OpenNet::ViewModels::Guide::GuideViewModel GuideView::ViewModel() { return GuideViewMvvmBase::ViewModel(); }
	void GuideView::ViewModel(OpenNet::ViewModels::Guide::GuideViewModel const& value) { GuideViewMvvmBase::ViewModel(value); }
	std::int32_t GuideView::StateIndex() { return static_cast<std::int32_t>(this->ViewModel().State()); }

	hstring GuideView::AllCulturesWelcomeText() { return m_allCulturesWelcomeText; }
	Windows::Foundation::Collections::IObservableVector<OpenNet::Models::NameCultureInfoValue> GuideView::Cultures() { return m_cultures; }
	OpenNet::Models::NameCultureInfoValue GuideView::SelectedCulture() { return this->ViewModel().SelectedCulture(); }
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
	bool GuideView::IsTermOfServiceAgreed() { return this->ViewModel().IsTermOfServiceAgreed(); }
	void GuideView::IsTermOfServiceAgreed(bool value) { this->ViewModel().IsTermOfServiceAgreed(value); }
	bool GuideView::IsPrivacyPolicyAgreed() { return this->ViewModel().IsPrivacyPolicyAgreed(); }
	void GuideView::IsPrivacyPolicyAgreed(bool value) { this->ViewModel().IsPrivacyPolicyAgreed(value); }
	bool GuideView::IsIssueReportAgreed() { return this->ViewModel().IsIssueReportAgreed(); }
	void GuideView::IsIssueReportAgreed(bool value) { this->ViewModel().IsIssueReportAgreed(value); }
	bool GuideView::IsOpenSourceLicenseAgreed() { return this->ViewModel().IsOpenSourceLicenseAgreed(); }
	void GuideView::IsOpenSourceLicenseAgreed(bool value) { this->ViewModel().IsOpenSourceLicenseAgreed(value); }
	bool GuideView::IsAgreementCopyAgreed() { return this->ViewModel().IsAgreementCopyAgreed(); }
	void GuideView::IsAgreementCopyAgreed(bool value) { this->ViewModel().IsAgreementCopyAgreed(value); }
	void GuideView::NextOrComplete(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		this->ViewModel().State(this->ViewModel().State() + 1);
	}
}
