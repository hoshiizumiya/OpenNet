#include "XamlWorkaround.h"
#include "InvokeCommandOnLoadedBehavior.h"
#if __has_include("/UI/Xaml/Behavior/InvokeCommandOnLoadedBehavior.g.cpp")
#include "/UI/Xaml/Behavior/InvokeCommandOnLoadedBehavior.g.cpp"
#endif

import winrt.XamlToolkit.WinUI.Behaviors;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Behavior::implementation
{

}
