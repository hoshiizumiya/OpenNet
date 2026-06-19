#pragma once

#include "ViewModels/Guide/GuideViewModel.g.h"
#include "mvvm_framework/view_model.h"

import winrt.OpenNet.Models;
import std;

namespace winrt::OpenNet::ViewModels::Guide::implementation
{
	struct GuideViewModel : GuideViewModelT<GuideViewModel>, ::mvvm::ViewModel<GuideViewModel>
	{
		GuideViewModel();

		std::uint32_t State();
		void State(std::uint32_t value);
		hstring NextOrCompleteButtonText();
		void NextOrCompleteButtonText(hstring const& value);
		bool IsNextOrCompleteButtonEnabled();
		void IsNextOrCompleteButtonEnabled(bool value);
		bool IsSetDataFolderEnabled();
		void IsSetDataFolderEnabled(bool value);
		hstring AllCulturesWelcomeText();
		void AllCulturesWelcomeText(hstring const& value);
		winrt::OpenNet::Models::NameCultureInfoValue SelectedCulture();
		void SelectedCulture(winrt::OpenNet::Models::NameCultureInfoValue const& value);
		bool IsTermOfServiceAgreed();
		void IsTermOfServiceAgreed(bool value);
		bool IsPrivacyPolicyAgreed();
		void IsPrivacyPolicyAgreed(bool value);
		bool IsIssueReportAgreed();
		void IsIssueReportAgreed(bool value);
		bool IsOpenSourceLicenseAgreed();
		void IsOpenSourceLicenseAgreed(bool value);
		bool IsAgreementCopyAgreed();
		void IsAgreementCopyAgreed(bool value);

	private:
		void OnAgreementStateChanged();
		std::uint32_t m_state{ 0 };
		hstring m_nextOrCompleteButtonText;
		bool m_isNextOrCompleteButtonEnabled{ false };
		bool m_isSetDataFolderEnabled{ false };
		hstring m_allCulturesWelcomeText;
		winrt::OpenNet::Models::NameCultureInfoValue m_selectedCulture{ nullptr };
		bool m_isTermOfServiceAgreed{ false };
		bool m_isPrivacyPolicyAgreed{ false };
		bool m_isIssueReportAgreed{ false };
		bool m_isOpenSourceLicenseAgreed{ false };
		bool m_isAgreementCopyAgreed{ false };
	};
}

namespace winrt::OpenNet::ViewModels::Guide::factory_implementation
{
	struct GuideViewModel : GuideViewModelT<GuideViewModel, implementation::GuideViewModel>
	{
	};
}
