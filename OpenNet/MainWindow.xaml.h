#pragma once

#include "ViewModels/MainViewModel.h"
#include "MainWindow.g.h"

using MainViewModel = winrt::OpenNet::ViewModels::MainViewModel;

namespace winrt::OpenNet::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // ViewModel
        winrt::OpenNet::ViewModels::MainViewModel ViewModel();

        // Page open helpers (refactored pattern)
        void openHomePage();
        void openContactsPage();
        void openTasksPage();
        void openFilesPage();
        void openNetworkSettingsPage();
        void openServersPage();
        void openRSSPage();
        void openSettingsPage();

        // Navigation
        void Navigate(winrt::hstring const& tag);

        // Event handlers (XAML wired)
        void NavView_ItemInvoked(winrt::Microsoft::UI::Xaml::Controls::NavigationView const&,
                                 winrt::Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const&);
        void NavFrame_Navigating(winrt::Windows::Foundation::IInspectable const&,
                                 winrt::Microsoft::UI::Xaml::Navigation::NavigatingCancelEventArgs const&);
        void NavFrame_Navigated(winrt::Windows::Foundation::IInspectable const&,
                                winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&);
        void AppTitleBar_BackRequested(winrt::Microsoft::UI::Xaml::Controls::TitleBar const&,
                                       winrt::Windows::Foundation::IInspectable const&);

        // (Optional) API parity with provided sample
        void SaveWindowState();
        void RestoreWindowState();

    private:
        // Window helpers
        void InitWindowStyle(winrt::Microsoft::UI::Xaml::Window const& window);

        // Selection update
        void UpdateNavigationSelection(winrt::hstring const& tag);

        // ViewModel
        winrt::OpenNet::ViewModels::MainViewModel m_viewModel{ nullptr };

        // (Future extension placeholders similar to sample)
        void LoadFavorites() {} // no-op for now
        bool m_favoritesLoaded{ false };
        void RootGridXamlRoot_Changed(winrt::Microsoft::UI::Xaml::XamlRoot sender, winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs args);

    public:
        //void NavView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);
        //void NavigateTo(hstring const& tag);
        void Grid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::OpenNet::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
