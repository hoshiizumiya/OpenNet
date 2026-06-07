#pragma once

#include "ViewModels/Guide/GuideViewModel.g.h"

#include "mvvm_framework/view_model.h"

namespace winrt::OpenNet::ViewModels::Guide::implementation
{
    struct GuideViewModel : GuideViewModelT<GuideViewModel>, ::mvvm::ViewModel<GuideViewModel>
    {
        GuideViewModel();

        std::uint32_t State();
        void State(std::uint32_t value);
        hstring AllCulturesWelcomeText();
        void AllCulturesWelcomeText(hstring const& value);
    };
}

namespace winrt::OpenNet::ViewModels::Guide::factory_implementation
{
    struct GuideViewModel : GuideViewModelT<GuideViewModel, implementation::GuideViewModel>
    {
    };
}
