export module OpenNet.Web.ServerDomainMode;

export namespace OpenNet::Web
{
	enum class ServerDomainMode
	{
		Primary,
		Backup,
		AutoDetect
	};
}