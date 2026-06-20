#include "XamlWorkaround.h"
#include "GuideViewModel.h"
#if __has_include("/ViewModels/Guide/GuideViewModel.g.cpp")
#include "/ViewModels/Guide/GuideViewModel.g.cpp"
#endif

import OpenNet.Core.Utils.Message;
import OpenNet.ViewModels.Guide.GuideState;
import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Core.Setting.LocalSetting;

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace ::OpenNet::Core::Setting;

namespace winrt::OpenNet::ViewModels::Guide::implementation
{
	GuideViewModel::GuideViewModel()
	{

	}

	std::uint32_t GuideViewModel::State()
	{
		// To speed up the app load speed, use WinRT api LocalSetting here
		::OpenNet::ViewModels::Guide::GuideState state = ::OpenNet::Core::Setting::LocalSetting::Get(SettingKeys::GuideState, ::OpenNet::ViewModels::Guide::GuideState::Language);

		switch (state)
		{
			case ::OpenNet::ViewModels::Guide::GuideState::Document:
				m_isTermOfServiceAgreed = false;
				m_isPrivacyPolicyAgreed = false;
				m_isIssueReportAgreed = false;
				m_isOpenSourceLicenseAgreed = false;
				m_isAgreementCopyAgreed = false;
				(m_nextOrCompleteButtonText, m_isNextOrCompleteButtonEnabled) = (ResourceGetString(L"ViewModelGuideActionNext"), false);
				break;
			case ::OpenNet::ViewModels::Guide::GuideState::Completed:
				(m_nextOrCompleteButtonText, m_isNextOrCompleteButtonEnabled) = (ResourceGetString(L"ViewModelGuideActionComplete"), true);
				break;
			default:
				(m_nextOrCompleteButtonText, m_isNextOrCompleteButtonEnabled) = (ResourceGetString(L"ViewModelGuideActionNext"), true);
				break;
		}

		return (std::uint32_t)state;
	}

	void GuideViewModel::State(std::uint32_t value)
	{
		throw hresult_not_implemented();
	}

	hstring GuideViewModel::NextOrCompleteButtonText()
	{
		return m_nextOrCompleteButtonText;
	}
	void GuideViewModel::NextOrCompleteButtonText(hstring const& value)
	{
		m_nextOrCompleteButtonText = value;
	}
	bool GuideViewModel::IsNextOrCompleteButtonEnabled()
	{
		return m_isNextOrCompleteButtonEnabled;
	}
	void GuideViewModel::IsNextOrCompleteButtonEnabled(bool value)
	{
		m_isNextOrCompleteButtonEnabled = value;
	}
	bool GuideViewModel::IsSetDataFolderEnabled()
	{
		return m_isSetDataFolderEnabled;
	}
	void GuideViewModel::IsSetDataFolderEnabled(bool value)
	{
		m_isSetDataFolderEnabled = value;
	}


	hstring GuideViewModel::AllCulturesWelcomeText()
	{
		return hstring();
	}

	void GuideViewModel::AllCulturesWelcomeText(hstring const& value)
	{
	}

	winrt::OpenNet::Models::NameCultureInfoValue GuideViewModel::SelectedCulture()
	{
		throw hresult_not_implemented();
	}
	void GuideViewModel::SelectedCulture(winrt::OpenNet::Models::NameCultureInfoValue const& value)
	{
		throw hresult_not_implemented();
	}
#pragma region Agreement

	bool GuideViewModel::IsTermOfServiceAgreed()
	{
		throw hresult_not_implemented();
	}
	void GuideViewModel::IsTermOfServiceAgreed(bool value)
	{
		throw hresult_not_implemented();
	}
	bool GuideViewModel::IsPrivacyPolicyAgreed()
	{
		throw hresult_not_implemented();
	}
	void GuideViewModel::IsPrivacyPolicyAgreed(bool value)
	{
		throw hresult_not_implemented();
	}
	bool GuideViewModel::IsIssueReportAgreed()
	{
		throw hresult_not_implemented();
	}
	void GuideViewModel::IsIssueReportAgreed(bool value)
	{
		throw hresult_not_implemented();
	}
	bool GuideViewModel::IsOpenSourceLicenseAgreed()
	{
		throw hresult_not_implemented();
	}
	void GuideViewModel::IsOpenSourceLicenseAgreed(bool value)
	{
		throw hresult_not_implemented();
	}
	bool GuideViewModel::IsAgreementCopyAgreed()
	{
		throw hresult_not_implemented();
	}
	void GuideViewModel::IsAgreementCopyAgreed(bool value)
	{
		throw hresult_not_implemented();
	}
#pragma endregion
	void GuideViewModel::OnAgreementStateChanged()
	{
	}

}
