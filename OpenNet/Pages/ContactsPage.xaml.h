#pragma once

#include "Pages/ContactsPage.g.h"

namespace winrt::OpenNet::Pages::implementation
{
    struct ContactsPage : ContactsPageT<ContactsPage>
    {
        ContactsPage();

    };
}

namespace winrt::OpenNet::Pages::factory_implementation
{
    struct ContactsPage : ContactsPageT<ContactsPage, implementation::ContactsPage>
    {
    };
}
