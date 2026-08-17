#pragma once

// Hand-written XAML implementation boundary. Win32 and the STL are established
// before importing C++/WinRT modules; unlike generated metadata sources, these
// files may still include Shell/COM/diagnostic SDK headers later in the TU.
#include "WindowsPlatform.h"

// Follow the same boundary used by CommunityToolkit.WinUI: establish only the
// native/STL headers needed by hand-written code, then let the generated-XAML
// workaround block textual headers before any named-module import occurs.
#include <cassert>
#include <functional>
#include <map>
#include <mutex>
#include <regex>

#ifndef WINRT_IMPORT_MODULE
#define WINRT_IMPORT_MODULE
#endif

// Hand-written XAML sources can still consume Shell/COM headers after this
// boundary. Do not fake their include guards here; only prevent generated
// fragments from expanding Microsoft STL headers after the named import.
#undef _STL_COMPILER_PREPROCESSOR
#define _STL_COMPILER_PREPROCESSOR 0

import std;
#include "winrt_module_imports.h"
import winrt.WinUI3Package;
import winrt.XamlToolkit.WinUI;
import winrt.XamlToolkit.WinUI.Controls;
import winrt.XamlToolkit.Labs.WinUI;
