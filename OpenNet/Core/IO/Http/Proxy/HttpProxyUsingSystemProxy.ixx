export module OpenNet.Core.IO.Http.Proxy.HttpProxyUsingSystemProxy;

import std;

export namespace OpenNet::Core::IO::Http
{
	class HttpProxyUsingSystemProxy
	{
	public:
		static std::wstring GetSystemProxyForUrl(const std::wstring& url);
	};
}