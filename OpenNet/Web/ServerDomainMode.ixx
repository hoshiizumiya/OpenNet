export module OpenNet.Web.ServerDomainMode;

export namespace OpenNet::Web
{
	enum class ServerDomainMode
	{
		AutoDetect,
		Ip,
		Http,
		Https
	};
}