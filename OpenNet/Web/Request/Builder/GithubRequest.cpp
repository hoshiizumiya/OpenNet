#include "pch.h"
#include "GithubRequest.h"

import winrt.Windows.Web.Http.Headers;

GithubRequest::GithubRequest(wchar_t const* url) :
	HttpRequestMessage{ winrt::Windows::Web::Http::HttpMethod::Get(), winrt::Windows::Foundation::Uri{url} }
{
	Headers().Append(L"User-Agent", L"OpenNetApp");
}