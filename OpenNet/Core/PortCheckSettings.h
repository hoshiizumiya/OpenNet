#pragma once

#include <cstdint>

namespace OpenNet::Core::PortCheckSettings
{
    inline constexpr char Category[] = "network";
    inline constexpr char IntervalMinutesKey[] = "port_check_interval_minutes";
    inline constexpr std::int64_t DefaultIntervalMinutes = 5;
    inline constexpr std::int64_t MinimumIntervalMinutes = 1;
    // Keep the countdown's seconds within the Int32 range.
    inline constexpr std::int64_t MaximumIntervalMinutes = 2147483647 / 60;

    constexpr std::int64_t NormalizeIntervalMinutes(std::int64_t value) noexcept
    {
        return value < MinimumIntervalMinutes ? MinimumIntervalMinutes :
            value > MaximumIntervalMinutes ? MaximumIntervalMinutes : value;
    }
}
