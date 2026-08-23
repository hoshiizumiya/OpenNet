#include "XamlWorkaround.h"
#include "GuideWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/GuideWindow.g.cpp")
#include "UI/Xaml/View/Windows/GuideWindow.g.cpp"
#endif

import OpenNet.App;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Setting.LocalSetting;
import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import OpenNet.ViewModels.Guide.GuideState;
import winrt.Microsoft.UI.Windowing;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	GuideWindow::GuideWindow()
	{
		ExtendsContentIntoTitleBar(true);
		::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(Window());
		Closed([this](auto const&, auto const&)
		{
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(Window());
		});
	}

	void GuideWindow::InitializeComponent()
	{
		GuideWindowT::InitializeComponent();
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(Window());
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(Window());
		bool const isFirstRun = !winrt::OpenNet::implementation::App::window;
		if (!isFirstRun)
		{
			GuideContent().ViewModel().State(static_cast<std::uint32_t>(
				::OpenNet::ViewModels::Guide::GuideState::Language));
		}
		Closed([isFirstRun](auto const&, auto const&)
		{
			auto const state = ::OpenNet::Core::Setting::LocalSetting::Get(
				::OpenNet::Core::Setting::SettingKeys::GuideState,
				::OpenNet::ViewModels::Guide::GuideState::Language);
			if (isFirstRun && state < ::OpenNet::ViewModels::Guide::GuideState::Completed)
			{
				winrt::OpenNet::implementation::App::RequestExit();
			}
			else if (!isFirstRun && state < ::OpenNet::ViewModels::Guide::GuideState::Completed)
			{
				// Opening the guide from the running app is only a preview.
				// Closing it early must not turn the next launch into first run.
				::OpenNet::Core::Setting::LocalSetting::Set(
					::OpenNet::Core::Setting::SettingKeys::GuideState,
					::OpenNet::ViewModels::Guide::GuideState::Completed);
			}
		});
	}

	void GuideWindow::OnGuideCompleted(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&)
	{
		auto strong = get_strong();
		winrt::OpenNet::implementation::App::CompleteFirstRun();
		Close();
	}
}
