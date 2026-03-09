#pragma once

#include "UI/Xaml/View/Dialog/CloseToTrayDialog.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
    struct CloseToTrayDialog : CloseToTrayDialogT<CloseToTrayDialog>
    {
        CloseToTrayDialog();

        bool RememberChoice();
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
    struct CloseToTrayDialog : CloseToTrayDialogT<CloseToTrayDialog, implementation::CloseToTrayDialog>
    {
    };
}
