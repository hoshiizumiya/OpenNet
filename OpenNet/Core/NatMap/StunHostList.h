#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace OpenNet::Core::NatMap
{
    // A compact, UI-friendly fallback list. All entries share the separately
    // configured STUN port; semicolons are deliberately used so IPv6 literals
    // can be supported later without changing the list syntax.
    inline std::vector<std::wstring> ParseStunHostList(std::wstring_view value)
    {
        constexpr std::size_t MaximumHosts = 8;
        constexpr std::size_t MaximumHostLength = 253;
        auto const whitespace = [](wchar_t character) noexcept
        {
            return character == L' ' || character == L'\t' || character == L'\r' || character == L'\n';
        };

        std::vector<std::wstring> hosts;
        std::size_t start{};
        while (start <= value.size())
        {
            std::size_t const delimiter = value.find(L';', start);
            std::size_t first = start;
            std::size_t last = delimiter == std::wstring_view::npos ? value.size() : delimiter;
            while (first < last && whitespace(value[first])) ++first;
            while (last > first && whitespace(value[last - 1])) --last;
            if (first == last || last - first > MaximumHostLength || hosts.size() == MaximumHosts) return {};
            hosts.emplace_back(value.substr(first, last - first));
            if (delimiter == std::wstring_view::npos) break;
            start = delimiter + 1;
        }
        return hosts;
    }
}
