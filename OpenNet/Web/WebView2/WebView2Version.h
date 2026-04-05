#pragma once

namespace OpenNet::Web::WebView2
{
	const struct WebView2Version
	{
		const winrt::hstring RawVersion;
		const winrt::hstring Version;
		const bool Supported;

		WebView2Version(const winrt::hstring& RawVersion, const winrt::hstring& Version, bool Supported)
			: RawVersion(RawVersion), Version(Version), Supported(Supported)
		{
		}
	};

}