#include "XamlWorkaround.h"
#include "DataTableSortViewModel.h"
#include "ViewModels/DataTableSortViewModel.g.cpp"

import OpenNet.Core.AppSettingsDatabase;

namespace winrt::OpenNet::ViewModels::implementation
{
    DataTableSortViewModel::DataTableSortViewModel(winrt::hstring const& tableKey) : m_tableKey(winrt::to_string(tableKey))
    {
        const auto saved = ::OpenNet::Core::AppSettingsDatabase::Instance().GetStringW("table_sort", m_tableKey);
        if (saved) m_state = ::OpenNet::ViewModels::DataTableSortState::Deserialize(*saved);

        m_toggleCommand = ::mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](winrt::Windows::Foundation::IInspectable const& parameter)
            {
                const auto column = winrt::unbox_value_or<winrt::hstring>(parameter, L"");
                if (column.empty()) return;
                if (auto self = weak.get())
                {
                    self->m_state.Toggle(std::wstring_view{ column });
                    self->PublishState();
                }
            })
            .Build();
    }

    void DataTableSortViewModel::Reset()
    {
        m_state = {};
        PublishState();
    }

    void DataTableSortViewModel::PublishState()
    {
        // One setting contains both fields: a restored arrow cannot disagree
        // with the restored column because of a partially written pair.
        ::OpenNet::Core::AppSettingsDatabase::Instance().SetStringW("table_sort", m_tableKey, m_state.Serialize());
        // Mutate both fields before notifying bindings and sorting adapters.
        RaisePropertyChanged(L"Column");
        RaisePropertyChanged(L"Direction");
    }
}
