#pragma once
#include "ViewModels/DataTableSortViewModel.g.h"
#include "ViewModels/DataTableSortState.h"
#include "mvvm_framework/delegate_command_builder.h"

import OpenNet.ViewModels.ObservableMixin;

namespace winrt::OpenNet::ViewModels::implementation
{
    struct DataTableSortViewModel : DataTableSortViewModelT<DataTableSortViewModel>, ::OpenNet::ViewModels::ObservableMixin<DataTableSortViewModel>
    {
        explicit DataTableSortViewModel(winrt::hstring const& tableKey);

        winrt::hstring Column() const { return winrt::hstring{ m_state.Column }; }
        int32_t Direction() const { return m_state.Direction; }
        winrt::Microsoft::UI::Xaml::Input::ICommand ToggleCommand() const { return m_toggleCommand; }
        void Reset();

    private:
        void PublishState();
        std::string m_tableKey;
        ::OpenNet::ViewModels::DataTableSortState m_state;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_toggleCommand{ nullptr };
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct DataTableSortViewModel : DataTableSortViewModelT<DataTableSortViewModel, implementation::DataTableSortViewModel>
    {
    };
}
