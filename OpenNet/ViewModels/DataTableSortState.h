#pragma once

// The caller provides <string>/<string_view> or import std before this header.
// Keep the transition and persistence format independent of WinRT and XAML.
namespace OpenNet::ViewModels
{
    struct DataTableSortState
    {
        std::wstring Column;
        int Direction{}; // 0: source order, 1: ascending, 2: descending

        void Toggle(std::wstring_view column)
        {
            if (column.empty()) return;
            Direction = Column == column ? (Direction + 1) % 3 : 1;
            Column = column;
        }

        std::wstring Serialize() const
        {
            return Column + L"|" + static_cast<wchar_t>(L'0' + Direction);
        }

        static DataTableSortState Deserialize(std::wstring_view value)
        {
            const auto separator = value.find(L'|');
            if (separator == std::wstring_view::npos || separator + 2 != value.size()) return {};
            const auto direction = value.back() - L'0';
            if (direction < 0 || direction > 2 || (separator == 0 && direction != 0)) return {};
            return { std::wstring{ value.substr(0, separator) }, direction };
        }
    };
}
