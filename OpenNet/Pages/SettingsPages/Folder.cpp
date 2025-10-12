#include "pch.h"
#include "Folder.h"
#if __has_include("Pages/SettingsPages/Folder.g.cpp")
#include "Pages/SettingsPages/Folder.g.cpp"
#endif

using namespace winrt;
using namespace winrt::OpenNet::Pages::SettingsPages::implementation;

Folder::Folder()
{
}

winrt::hstring Folder::Name()
{
    return m_name;
}

void Folder::Name(winrt::hstring const& value)
{
    m_name = value;
}
