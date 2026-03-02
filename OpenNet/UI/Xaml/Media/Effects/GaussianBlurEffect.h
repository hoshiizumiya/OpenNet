#pragma once
// Re-exports Win2D effect types for use in the OpenNet pipeline system.
// Instead of manually implementing IGraphicsEffectD2D1Interop,
// we use Win2D's built-in effect classes which properly implement the interface.

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Windows.Graphics.Effects.h>
