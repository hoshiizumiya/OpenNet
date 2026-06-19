import winrt.Windows.Web.Http;

class GithubRequest : public winrt::Windows::Web::Http::HttpRequestMessage
{
public:
	GithubRequest(wchar_t const* url);
};