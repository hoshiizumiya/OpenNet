#include "UI/Xaml/Control/Theme/LiquidGlassParameterState.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using OpenNet::UI::LiquidGlassParameterState;
#define CHECK(expression) do { if (!(expression)) throw std::runtime_error(#expression); } while (false)

int main()
{
    try
    {
        LiquidGlassParameterState sharpness(0.25, 64, 1.6);
        CHECK(!sharpness.Load(0.1));
        CHECK(sharpness.Value() == 1.6 && sharpness.Committed() == 1.6);
        for (double rejected : {0.0, 0.249, 64.01, std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            CHECK(!sharpness.Stage(rejected));
            CHECK(sharpness.Value() == 1.6 && !sharpness.Pending());
        }
        CHECK(sharpness.Stage(0.25));
        CHECK(sharpness.Pending() && sharpness.Committed() == 1.6);
        sharpness.Commit();
        CHECK(!sharpness.Pending() && sharpness.Committed() == 0.25);
        CHECK(sharpness.Stage(64));
        CHECK(sharpness.RestoreRejected()); // Rejected edit restores the saved baseline first.
        CHECK(sharpness.Value() == 0.25 && sharpness.Committed() == 0.25);
        CHECK(sharpness.RestoreRejected()); // A stale/rejected saved baseline restores this preset only.
        CHECK(sharpness.Value() == 1.6 && sharpness.Committed() == 1.6 && !sharpness.Overridden());
        CHECK(!sharpness.RestoreRejected()); // The caller must disable a brush rejecting its own preset.

        LiquidGlassParameterState width(0.25, 32, 1);
        CHECK(!width.Stage(0.1) && width.Stage(32));
        width.Commit();
        width.Reset();
        CHECK(width.Value() == 1 && width.Committed() == 32 && width.Pending());
        width.Rollback();
        CHECK(width.Value() == 32 && !width.Pending());

        LiquidGlassParameterState other(0, 4, 0.4);
        CHECK(other.Stage(3.5));
        CHECK(sharpness.Stage(5));
        CHECK(sharpness.RestoreRejected());
        CHECK(other.Value() == 3.5 && other.Committed() == 0.4 && other.Pending());
        CHECK(sharpness.Revision() > 0);
        using OpenNet::UI::ApplyLiquidGlassParameter;
        using Result = OpenNet::UI::LiquidGlassParameterApplyResult;
        LiquidGlassParameterState native(0.25, 64, 1.6);
        CHECK(native.Load(2));
        CHECK(native.Stage(64));
        int writes = 0;
        CHECK(ApplyLiquidGlassParameter(native, 1.6, [&](double value) { ++writes; return value <= 4; }) == Result::Recovered);
        CHECK(writes == 2 && native.Value() == 2 && native.Committed() == 2);
        CHECK(native.Load(5)); // Simulate a saved baseline rejected after a library upgrade.
        CHECK(ApplyLiquidGlassParameter(native, 1.6, [](double value) { return value <= 4; }) == Result::Recovered);
        CHECK(native.Value() == 1.6 && native.Committed() == 1.6);
        CHECK(ApplyLiquidGlassParameter(native, 0.1, [](double value) { return value >= 0.25; }) == Result::Recovered);
        writes = 0;
        CHECK(ApplyLiquidGlassParameter(native, 1.6, [&](double) { ++writes; return false; }) == Result::Unsupported);
        CHECK(writes == 1);
        bool unexpectedPropagated = false;
        try { (void)ApplyLiquidGlassParameter(native, 1.6, [](double) -> bool { throw std::runtime_error("unexpected native failure"); }); }
        catch (std::runtime_error const&) { unexpectedPropagated = true; }
        CHECK(unexpectedPropagated);
        std::cout << "PASS: bounds/non-finite values, legacy values, staged commits, rollback, native rejection and profile isolation\n";
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
