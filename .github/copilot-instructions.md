# Copilot Instructions

## General Guidelines
- First general instruction
- Second general instruction
- When updating documentation, explicitly ensure the user can see the actual edits in the target markdown file (avoid claiming changes without applying them).

## Code Style
- Use specific formatting rules
- Follow naming conventions

## Project-Specific Rules
- TitleCard is implemented as a custom Control with its template defined in a separate resource dictionary file named `TitleCard_ResourceDictionary.xaml` (not a `TitleCard.xaml` control markup file).
- For C++ WinRT, the IDL namespace `OpenNet.UI.Xaml.Behaviors` maps to the generated file path `UI/Xaml/Behaviors/XXX.g.h`. The generated `.g.h` files are placed according to the namespace structure in the Generated Files directory, regardless of the subdirectory of the `.idl` file. In the corresponding `.h` files, include using `#include "UI/Xaml/Behaviors/XXX.g.h"` instead of relative paths.