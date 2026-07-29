#include "XamlWorkaround.h"
#include "GuideWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/GuideWindow.g.cpp")
#include "UI/Xaml/View/Windows/GuideWindow.g.cpp"
#endif

import OpenNet.App;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Windowing;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	GuideWindow::GuideWindow()
	{
		ExtendsContentIntoTitleBar(true);
	}

	void GuideWindow::InitializeComponent()
	{
		GuideWindowT::InitializeComponent();
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(*this);
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		if (database.GetBool("webui_host", "initialized").value_or(false))
		{
			GuideContent().ViewModel().State(0);
		}
		Closed([](auto const&, auto const&)
		{
			auto& settings =
				::OpenNet::Core::AppSettingsDatabase::Instance();
			settings.Initialize();
			if (!settings.GetBool(
				"webui_host", "initialized").value_or(false))
			{
				winrt::OpenNet::implementation::App::RequestExit();
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
