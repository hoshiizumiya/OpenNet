#include "XamlWorkaround.h"
#include "GuideViewModel.h"
#if __has_include("ViewModels/Guide/GuideViewModel.g.cpp")
#include "ViewModels/Guide/GuideViewModel.g.cpp"
#endif

import OpenNet.Core.Utils.Message;
import OpenNet.ViewModels.Guide.GuideState;
import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Core.Setting.LocalSetting;
import winrt.Windows.Globalization;

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace ::OpenNet::Core::Setting;

namespace winrt::OpenNet::ViewModels::Guide::implementation
{
	GuideViewModel::GuideViewModel()
	{
		m_state = static_cast<std::uint32_t>(LocalSetting::Get(
			SettingKeys::GuideState,
			::OpenNet::ViewModels::Guide::GuideState::Language));
		m_nextOrCompleteButtonText = ResourceGetString(L"ViewModelGuideActionNext");
		m_isNextOrCompleteButtonEnabled = m_state != static_cast<std::uint32_t>(::OpenNet::ViewModels::Guide::GuideState::Document);
		m_isSetDataFolderEnabled = true;
	}

	std::uint32_t GuideViewModel::State()
	{
		return m_state;
	}

	void GuideViewModel::State(std::uint32_t value)
	{
		auto const completed = static_cast<std::uint32_t>(::OpenNet::ViewModels::Guide::GuideState::Completed);
		value = std::min(value, completed);
		if (!SetProperty(m_state, value, L"State")) return;

		LocalSetting::Set(SettingKeys::GuideState, value);
		if (value == static_cast<std::uint32_t>(::OpenNet::ViewModels::Guide::GuideState::Document))
		{
			IsTermOfServiceAgreed(false);
			IsPrivacyPolicyAgreed(false);
			IsIssueReportAgreed(false);
			IsOpenSourceLicenseAgreed(false);
			IsAgreementCopyAgreed(false);
		}
		NextOrCompleteButtonText(value == completed
								 ? ResourceGetString(L"ViewModelGuideActionComplete")
								 : ResourceGetString(L"ViewModelGuideActionNext"));
	}

	hstring GuideViewModel::NextOrCompleteButtonText()
	{
		return m_nextOrCompleteButtonText;
	}
	void GuideViewModel::NextOrCompleteButtonText(hstring const& value)
	{
		SetProperty(m_nextOrCompleteButtonText, value, L"NextOrCompleteButtonText");
	}
	bool GuideViewModel::IsNextOrCompleteButtonEnabled()
	{
		return m_isNextOrCompleteButtonEnabled;
	}
	void GuideViewModel::IsNextOrCompleteButtonEnabled(bool value)
	{
		SetProperty(m_isNextOrCompleteButtonEnabled, value, L"IsNextOrCompleteButtonEnabled");
	}
	bool GuideViewModel::IsSetDataFolderEnabled()
	{
		return m_isSetDataFolderEnabled;
	}
	void GuideViewModel::IsSetDataFolderEnabled(bool value)
	{
		SetProperty(m_isSetDataFolderEnabled, value, L"IsSetDataFolderEnabled");
	}


	hstring GuideViewModel::AllCulturesWelcomeText()
	{
		return m_allCulturesWelcomeText;
	}

	void GuideViewModel::AllCulturesWelcomeText(hstring const& value)
	{
		SetProperty(m_allCulturesWelcomeText, value, L"AllCulturesWelcomeText");
	}

	winrt::OpenNet::Models::NameCultureInfoValue GuideViewModel::SelectedCulture()
	{
		return m_selectedCulture;
	}
	void GuideViewModel::SelectedCulture(winrt::OpenNet::Models::NameCultureInfoValue const& value)
	{
		if (!value || !SetProperty(m_selectedCulture, value, L"SelectedCulture")) return;
		winrt::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride(value.Value());
	}

#pragma region Agreement
	bool GuideViewModel::IsTermOfServiceAgreed()
	{
		return m_isTermOfServiceAgreed;
	}
	void GuideViewModel::IsTermOfServiceAgreed(bool value)
	{
		if (SetProperty(m_isTermOfServiceAgreed, value, L"IsTermOfServiceAgreed")) OnAgreementStateChanged();
	}
	bool GuideViewModel::IsPrivacyPolicyAgreed()
	{
		return m_isPrivacyPolicyAgreed;
	}
	void GuideViewModel::IsPrivacyPolicyAgreed(bool value)
	{
		if (SetProperty(m_isPrivacyPolicyAgreed, value, L"IsPrivacyPolicyAgreed")) OnAgreementStateChanged();
	}
	bool GuideViewModel::IsIssueReportAgreed()
	{
		return m_isIssueReportAgreed;
	}
	void GuideViewModel::IsIssueReportAgreed(bool value)
	{
		if (SetProperty(m_isIssueReportAgreed, value, L"IsIssueReportAgreed")) OnAgreementStateChanged();
	}
	bool GuideViewModel::IsOpenSourceLicenseAgreed()
	{
		return m_isOpenSourceLicenseAgreed;
	}
	void GuideViewModel::IsOpenSourceLicenseAgreed(bool value)
	{
		if (SetProperty(m_isOpenSourceLicenseAgreed, value, L"IsOpenSourceLicenseAgreed")) OnAgreementStateChanged();
	}
	bool GuideViewModel::IsAgreementCopyAgreed()
	{
		return m_isAgreementCopyAgreed;
	}
	void GuideViewModel::IsAgreementCopyAgreed(bool value)
	{
		if (SetProperty(m_isAgreementCopyAgreed, value, L"IsAgreementCopyAgreed")) OnAgreementStateChanged();
	}
#pragma endregion
	void GuideViewModel::OnAgreementStateChanged()
	{
		auto const current = LocalSetting::Get(
			SettingKeys::GuideState,
			::OpenNet::ViewModels::Guide::GuideState::Language);
		bool enabled = m_isTermOfServiceAgreed
			&& m_isPrivacyPolicyAgreed
			&& m_isIssueReportAgreed
			&& m_isOpenSourceLicenseAgreed;
		if (current == ::OpenNet::ViewModels::Guide::GuideState::Document) enabled = enabled && m_isAgreementCopyAgreed;
		IsNextOrCompleteButtonEnabled(enabled);
	}

}
