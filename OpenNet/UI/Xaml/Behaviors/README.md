# StickyHeaderBehavior Migration to C++/WinRT

## Overview

This document describes the complete migration of the `StickyHeaderBehavior` class hierarchy from C# to C++/WinRT, following the Windows Community Toolkit's design patterns.

## Architecture

The implementation consists of three key classes:

### 1. BehaviorBase
**Location**: `OpenNet\UI\Xaml\Behaviors\BehaviorBase.h/cpp`

Base class that manages the lifecycle of behavior attachment and initialization with lazy initialization support.

**Key Features**:
- Prevents duplicate initialization
- Subscribes to `SizeChanged` event for lazy initialization
- Tracks attachment state with `_isAttached` and `_isAttaching` flags

**Protected Methods**:
- `Initialize()`: Override to implement custom initialization logic (returns bool)
- `Uninitialize()`: Override to implement cleanup logic (returns bool)
- `OnAttached()`: Called after successful initialization
- `OnDetaching()`: Called before deinitialization
- `OnAssociatedObjectLoaded()`: Called when the associated object is loaded
- `OnAssociatedObjectUnloaded()`: Called when the associated object is unloaded

### 2. HeaderBehaviorBase
**Location**: `OpenNet\UI\Xaml\Behaviors\Headers\HeaderBehaviorBase.h/cpp`

Base class for header-related behaviors that work with ListView/GridView controls.

**Key Features**:
- Manages ScrollViewer discovery and manipulation
- Provides composition animation infrastructure
- Handles focus and size change events

**Protected Members**:
- `m_scrollViewer`: The ScrollViewer associated with the ListViewBase
- `m_scrollProperties`: Composition property set for scroll viewer
- `m_animationProperties`: Composition property set for animations
- `m_headerVisual`: Visual object for the header element

**Protected Methods**:
- `AssignAnimation()`: Set up composition animations (override in derived classes)
- `StopAnimation()`: Stop running animations (override in derived classes)
- `RemoveAnimation()`: Clean up animation event handlers

### 3. StickyHeaderBehavior
**Location**: `OpenNet\UI\Xaml\Behaviors\Headers\StickyHeaderBehavior.h/cpp`

Implements sticky header animation that keeps a header element visible at the top while scrolling.

**Public Methods**:
- `Show()`: Reset the header offset to 0 (brings header back to top)

**Implementation Details**:
- Uses `ExpressionAnimation` with the formula: `Max(OffsetY - ScrollViewer.Translation.Y, 0)`
- This keeps the header pinned to the top of the scroll viewport
- Supports smooth composition-based animation

## Usage

### In XAML

```xaml
<ListView x:Name="MyListView">
    <ListView.Header>
        <Border x:Name="HeaderBorder" Height="50">
            <!-- Header content -->
        </Border>
    </ListView.Header>
    <!-- List items -->
</ListView>
```

### In C++/WinRT Code

```cpp
#include "UI/Xaml/Behaviors/Headers/StickyHeaderBehavior.h"

// Create and initialize the behavior
auto headerElement = MyListView().Header().try_as<FrameworkElement>();
auto behavior = winrt::OpenNet::StickyHeaderBehavior();

// Initialize behavior (in real implementation, you'd attach it to the element)
if (behavior.Initialize())
{
    // Behavior is ready
}

// Later, if you need to show the header (reset scroll offset):
behavior.Show();
```

## Namespace

All classes are in the `winrt::OpenNet` namespace:

```cpp
using namespace winrt::OpenNet;

auto behavior = StickyHeaderBehavior();
auto headerBehavior = HeaderBehaviorBase();
```

## Helper Classes

### VisualTreeHelperEx
**Location**: `OpenNet\UI\Xaml\Behaviors\VisualTreeHelperEx.h`

Provides helper methods for working with the XAML visual tree:

```cpp
template<typename T>
static T FindAscendant(Microsoft::UI::Xaml::UIElement const& element)
```

Traverses up the visual tree to find an ancestor of type T.

## Composition Animation

The sticky header uses WinUI 3 Composition API:

- **Animation Target**: `Offset.Y` property of the header visual
- **Expression**: References scroll properties and animation properties
- **Parameters**:
  - `animProps.OffsetY`: Current animation offset
  - `scrollProps.Translation.Y`: Current scroll offset

## Building

The project builds successfully with Visual Studio 2026 and the Windows App SDK.

Compile time:
- No C++ compilation errors
- XAML markup errors resolved by removing placeholder styles
- Full support for WinUI 3 APIs

## References

- Original C# implementation: Windows Community Toolkit
- WinUI 3 Documentation: https://docs.microsoft.com/en-us/windows/windows-app-sdk/
- Composition API: https://docs.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.composition
