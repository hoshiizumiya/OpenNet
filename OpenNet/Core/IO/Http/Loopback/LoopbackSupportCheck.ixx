export module OpenNet.Core.IO.Http.Loopback.LoopbackSupportCheck;

import std;

export namespace OpenNet::Core::IO::Http
{
	class LoopbackSupportCheck
	{
	public:
		static bool IsLoopbackExempt(const std::wstring_view& familyName, const std::wstring_view& sid);
		static bool AddLoopbackExempt(const std::wstring_view& familyName, const std::wstring_view& sid);
		static bool IsPublicFirewallEnabled();
	};
}