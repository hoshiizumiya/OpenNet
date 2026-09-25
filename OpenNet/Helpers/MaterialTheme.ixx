export module OpenNet.Helpers.MaterialTheme;

import OpenNet.Core.AppSettingsDatabase;
import std;
import winrt.Windows.Foundation;
import winrt.Microsoft.UI.Xaml;
import winrt.WinUI.Composition.Hlsl;
import winrt.OpenNet.UI.Xaml.Control.Theme;

export namespace OpenNet::Helpers
{
	enum class MaterialStyle
	{
		Standard,
		LiquidGlass,
	};

	class MaterialTheme
	{
	public:
		static MaterialStyle Current()
		{
			auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
			return db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "material_style", 0) == 1
				? MaterialStyle::LiquidGlass
				: MaterialStyle::Standard;
		}

		static void Apply()
		{
			Apply(Current());
		}

		static void Apply(MaterialStyle style)
		{
			auto const app = winrt::Microsoft::UI::Xaml::Application::Current();
			if (!app) return;
			for (auto const& dictionary : app.Resources().MergedDictionaries())
			{
				auto const stateKey = winrt::box_value(L"MaterialControlState");
				if (dictionary.HasKey(stateKey))
				{
					auto const state = dictionary.Lookup(stateKey).try_as<winrt::OpenNet::UI::Xaml::Control::Theme::MaterialControlState>();
					if (state)
						state.StyleIndex(static_cast<std::int32_t>(style));
				}
				for (auto const& variant : dictionary.ThemeDictionaries())
				{
					auto const values = variant.Value().try_as<winrt::Microsoft::UI::Xaml::ResourceDictionary>();
					if (!values)
						continue;
					for (auto const key : { L"OpenNetTitleCardBrush", L"OpenNetCardBrush", L"OpenNetSurfaceBrush", L"OpenNetGuideLiquidGlassBrush" })
					{
						auto const boxedKey = winrt::box_value(key);
						if (!values.HasKey(boxedKey))
							continue;
						auto const brush = values.Lookup(boxedKey).try_as<winrt::WinUI::Composition::Hlsl::LiquidGlassBrush>();
						if (brush)
							brush.IsEnabled(style == MaterialStyle::LiquidGlass);
					}
				}
			}
		}

		static void Set(MaterialStyle style)
		{
			::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "material_style", static_cast<int>(style));
			Apply(style);
		}
	};
}
