#pragma once
#include "pch.h"
#include "Pages/NetworkSettingsPage.g.h"
#include "ViewModels/NetworkSettingsViewModel.h"

namespace winrt::OpenNet::Pages::implementation
{
    struct NetworkSettingsPage : NetworkSettingsPageT<NetworkSettingsPage>
    {
        NetworkSettingsPage();

        // IDL: OpenNet.ViewModels.NetworkSettingsViewModel ViewModel { get; }
        winrt::OpenNet::ViewModels::NetworkSettingsViewModel ViewModel() { return m_viewModel ? m_viewModel : (m_viewModel = winrt::OpenNet::ViewModels::NetworkSettingsViewModel()); }

    private:
        winrt::OpenNet::ViewModels::NetworkSettingsViewModel m_viewModel{ nullptr };
    };
}
namespace winrt::OpenNet::Pages::factory_implementation
{
    struct NetworkSettingsPage : NetworkSettingsPageT<NetworkSettingsPage, implementation::NetworkSettingsPage>
    {
    };
}
