#include "pch.h"
#include "GuideViewModel.h"
#if __has_include("/ViewModels/Guide/GuideViewModel.g.cpp")
#include "/ViewModels/Guide/GuideViewModel.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::ViewModels::Guide::implementation
{
    GuideViewModel::GuideViewModel()
    {

    }

    std::uint32_t GuideViewModel::State()
    {
        throw hresult_not_implemented();
    }

    void GuideViewModel::State(std::uint32_t value)
    {
        throw hresult_not_implemented();
    }
}
