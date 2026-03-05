#pragma once

#include "UI/Xaml/View/Pages/ContactsPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct ContactsPage : ContactsPageT<ContactsPage>
    {
        ContactsPage();

    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct ContactsPage : ContactsPageT<ContactsPage, implementation::ContactsPage>
    {
    };
}
