#include <stdexcept>
#include <string>
#include <string_view>
#include <iostream>

#include "../OpenNet/ViewModels/DataTableSortState.h"

namespace
{
    void Require(bool condition, char const* message)
    {
        if (!condition) throw std::runtime_error{ message };
    }
}

int main()
{
    using OpenNet::ViewModels::DataTableSortState;
    try
    {
        for (const std::wstring_view column : { L"Name", L"Progress", L"RemoteDLSpeed", L"TimeRemaining", L"Priority", L"文件名称" })
        {
            DataTableSortState state;
            for (const int expected : { 1, 2, 0, 1 })
            {
                state.Toggle(column);
                // A new page reads the persisted value, then receives the
                // next click. Exercise the transition across every recreation.
                state = DataTableSortState::Deserialize(state.Serialize());
                Require(state.Column == column, "Navigation lost the active column");
                Require(state.Direction == expected, "Navigation changed the sort cycle");
            }
            state.Toggle(L"Size");
            Require(state.Column == L"Size" && state.Direction == 1, "A different column must start ascending");
            state.Toggle(L"");
            Require(state.Column == L"Size" && state.Direction == 1, "An empty command parameter must be ignored");
            state = {};
            state = DataTableSortState::Deserialize(state.Serialize());
            Require(state.Column.empty() && state.Direction == 0, "Reset must survive navigation");
        }

        for (const std::wstring_view malformed : { L"", L"Name", L"Name|3", L"Name|-1", L"Name|12", L"Name|x", L"|1", L"Name|extra|1" })
        {
            const auto state = DataTableSortState::Deserialize(malformed);
            Require(state.Column.empty() && state.Direction == 0, "Malformed settings must fall back to source order");
        }
        std::cout << "DataTable sort-state regression tests passed.\n";
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
