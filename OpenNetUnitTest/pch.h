#pragma once
#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

import winrt.Windows.Foundation;
import winrt.Windows.Foundation.Collections;
import winrt.Windows.ApplicationModel.Activation;
import winrt.Microsoft.UI.Composition;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Controls.Primitives;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Interop;
import winrt.Microsoft.UI.Xaml.Markup;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Xaml.Navigation;
import winrt.Microsoft.UI.Xaml.Shapes;
import winrt.Microsoft.UI.Dispatching;
