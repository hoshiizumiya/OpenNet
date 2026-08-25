export module OpenNet.Helpers.WindowExBase;

import OpenNet.Helpers.WindowHelper;
import winrt.WinUI3Package;

export template <typename Derived>
class WindowExBase
{
protected:
	void InitializeWindowExBase(
		bool const trackWindow = true,
		bool const restorePlacement = true)
	{
		auto* derived = static_cast<Derived*>(this);
		auto projected = static_cast<typename Derived::class_type>(*derived);
		auto window = projected.template as<winrt::WinUI3Package::WindowEx>();
		window.HasBorder(true);
		window.HasTitleBar(true);
		window.IsResizable(true);
		window.IsMinimizable(true);
		window.IsMaximizable(true);
		if (trackWindow)
		{
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
			if (restorePlacement)
			{
				::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(window.Window(), winrt::get_class_name(projected));
			}
		}
	}
};
