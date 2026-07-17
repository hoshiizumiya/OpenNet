#pragma once

#include "mvvm_framework/view.h"

#include "ViewModels/Guide/GuideViewModel.h"
#include "Models/NameCultureInfoValue.h"
#include "UI/Xaml/View/GuideView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	struct GuideView;
	using GuideViewMvvmBase = ::mvvm::view<GuideView, winrt::OpenNet::ViewModels::Guide::GuideViewModel>;

	struct GuideView :
		GuideViewT<GuideView>,
		GuideViewMvvmBase
	{
		GuideView();
		winrt::OpenNet::ViewModels::Guide::GuideViewModel ViewModel();
		void ViewModel(winrt::OpenNet::ViewModels::Guide::GuideViewModel const& value);
		std::int32_t StateIndex();
		winrt::hstring AllCulturesWelcomeText();
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::Models::NameCultureInfoValue> Cultures();
		winrt::OpenNet::Models::NameCultureInfoValue SelectedCulture();
		void SelectedCulture(winrt::OpenNet::Models::NameCultureInfoValue const& value);
		bool AgreementUseGrid();
		winrt::hstring AgreementCopyTarget();
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
		void NextOrComplete(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::Models::NameCultureInfoValue> m_cultures;
		winrt::hstring m_allCulturesWelcomeText;


	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct GuideView : GuideViewT<GuideView, implementation::GuideView>
	{
	};
}
