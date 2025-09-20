#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "Pages/HomePage.xaml.h"
#include "Pages/ContactsPage.xaml.h"
#include "Pages/TasksPage.xaml.h"
#include "Pages/FilesPage.xaml.h"
#include "Pages/NetworkSettingsPage.xaml.h"
#include "Pages/ServersPage.xaml.h"
#include "Pages/SettingsPage.xaml.h"
#include "ViewModels/MainViewModel.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using MainViewModel = winrt::OpenNet::ViewModels::MainViewModel;

namespace winrt::OpenNet::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        SetTitleBar(AppTitleBar());
        InitWindowStyle(*this);

        m_viewModel = MainViewModel{};
        m_viewModel.Initialize();

        NavFrame().Navigated({ this, &MainWindow::NavFrame_Navigated });
        NavFrame().Navigating({ this, &MainWindow::NavFrame_Navigating });
        NavView().ItemInvoked({ this, &MainWindow::NavView_ItemInvoked });

        openHomePage();
    }

    MainViewModel MainWindow::ViewModel()
    {
        return m_viewModel;
    }

    IAsyncAction MainWindow::SetIconAsync(Microsoft::UI::Windowing::AppWindow window)
    {
        if (!window) co_return;
        try { /* set icon placeholder */ } catch (...) {}
        co_return;
    }

    void MainWindow::InitWindowStyle(Window const& window)
    {
        window.ExtendsContentIntoTitleBar(true);
        if (auto appWindow = window.AppWindow())
        {
            appWindow.TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
            SetIconAsync(appWindow);
        }
    }

    void MainWindow::SaveWindowState() {}
    void MainWindow::RestoreWindowState() {}

    void MainWindow::UpdateNavigationSelection(hstring const& tag)
    {
        if (tag.empty()) return;
        auto nav = NavView();
        for (auto const& obj : nav.MenuItems())
        {
            if (auto nvi = obj.try_as<NavigationViewItem>())
            {
                if (unbox_value_or<hstring>(nvi.Tag(), L"") == tag)
                {
                    if (nav.SelectedItem() != nvi) nav.SelectedItem(nvi);
                    return;
                }
            }
        }
        for (auto const& obj : nav.FooterMenuItems())
        {
            if (auto nvi = obj.try_as<NavigationViewItem>())
            {
                auto itemTag = unbox_value_or<hstring>(nvi.Tag(), L"");
                if (itemTag == tag || (tag == L"settings" && itemTag == L"Settings"))
                {
                    if (nav.SelectedItem() != nvi) nav.SelectedItem(nvi);
                    return;
                }
            }
        }
    }

    void MainWindow::openHomePage()
    {
        if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::Pages::HomePage>()) { UpdateNavigationSelection(L"home"); return; }
        NavFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::HomePage>());
        UpdateNavigationSelection(L"home");
    }
    void MainWindow::openContactsPage()
    {
        if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::Pages::ContactsPage>()) { UpdateNavigationSelection(L"contacts"); return; }
        NavFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::ContactsPage>());
        UpdateNavigationSelection(L"contacts");
    }
    void MainWindow::openTasksPage()
    {
        if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::Pages::TasksPage>()) { UpdateNavigationSelection(L"tasks"); return; }
        NavFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::TasksPage>());
        UpdateNavigationSelection(L"tasks");
    }
    void MainWindow::openFilesPage()
    {
        if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::Pages::FilesPage>()) { UpdateNavigationSelection(L"files"); return; }
        NavFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::FilesPage>());
        UpdateNavigationSelection(L"files");
    }
    void MainWindow::openNetworkSettingsPage()
    {
        if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::Pages::NetworkSettingsPage>()) { UpdateNavigationSelection(L"net"); return; }
        NavFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::NetworkSettingsPage>());
        UpdateNavigationSelection(L"net");
    }
    void MainWindow::openServersPage()
    {
        if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::Pages::ServersPage>()) { UpdateNavigationSelection(L"servers"); return; }
        NavFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::ServersPage>());
        UpdateNavigationSelection(L"servers");
    }
    void MainWindow::openSettingsPage()
    {
        if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::Pages::SettingsPage>()) { UpdateNavigationSelection(L"Settings"); return; }
        NavFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::SettingsPage>());
        UpdateNavigationSelection(L"Settings");
    }

    void MainWindow::NavView_ItemInvoked(NavigationView const&, NavigationViewItemInvokedEventArgs const& args)
    {
        hstring tag;
        if (auto container = args.InvokedItemContainer())
        {
            tag = unbox_value_or<hstring>(container.Tag(), L"");
        }
        Navigate(tag);
    }

    void MainWindow::Navigate(hstring const& tag)
    {
        auto weak = get_weak();
        this->DispatcherQueue().TryEnqueue([weak, tag]() {
            if (auto self = weak.get())
            {
                auto frame = self->NavFrame();
                auto content = frame.Content();

                if (tag == L"home") { if (content && content.try_as<winrt::OpenNet::Pages::HomePage>()) return; self->openHomePage(); return; }
                if (tag == L"contacts") { if (content && content.try_as<winrt::OpenNet::Pages::ContactsPage>()) return; self->openContactsPage(); return; }
                if (tag == L"tasks") { if (content && content.try_as<winrt::OpenNet::Pages::TasksPage>()) return; self->openTasksPage(); return; }
                if (tag == L"files") { if (content && content.try_as<winrt::OpenNet::Pages::FilesPage>()) return; self->openFilesPage(); return; }
                if (tag == L"net") { if (content && content.try_as<winrt::OpenNet::Pages::NetworkSettingsPage>()) return; self->openNetworkSettingsPage(); return; }
                if (tag == L"servers") { if (content && content.try_as<winrt::OpenNet::Pages::ServersPage>()) return; self->openServersPage(); return; }
                if (tag == L"settings" || tag == L"Settings") { if (content && content.try_as<winrt::OpenNet::Pages::SettingsPage>()) return; self->openSettingsPage(); return; }

                if (content && content.try_as<winrt::OpenNet::Pages::HomePage>()) return; 
                self->openHomePage();
            }
        });
    }

    void MainWindow::NavFrame_Navigating(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigatingCancelEventArgs const&)
    {
    }

    void MainWindow::AppTitleBar_BackRequested(Microsoft::UI::Xaml::Controls::TitleBar const&, IInspectable const&)
    {
        if (NavFrame().CanGoBack())
        {
            NavFrame().GoBack();
        }
    }

    void MainWindow::NavFrame_Navigated(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
    {
        auto name = e.SourcePageType().Name;
        hstring tag;
        if (name == xaml_typename<winrt::OpenNet::Pages::HomePage>().Name) tag = L"home";
        else if (name == xaml_typename<winrt::OpenNet::Pages::ContactsPage>().Name) tag = L"contacts";
        else if (name == xaml_typename<winrt::OpenNet::Pages::TasksPage>().Name) tag = L"tasks";
        else if (name == xaml_typename<winrt::OpenNet::Pages::FilesPage>().Name) tag = L"files";
        else if (name == xaml_typename<winrt::OpenNet::Pages::NetworkSettingsPage>().Name) tag = L"net";
        else if (name == xaml_typename<winrt::OpenNet::Pages::ServersPage>().Name) tag = L"servers";
        else if (name == xaml_typename<winrt::OpenNet::Pages::SettingsPage>().Name) tag = L"Settings";
        if (!tag.empty()) UpdateNavigationSelection(tag);
    }
}
