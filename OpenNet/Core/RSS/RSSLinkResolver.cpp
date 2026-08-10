module OpenNet.Core.RSS.RSSLinkResolver;

import winrt.Windows.Foundation;
import winrt.Windows.Storage;
import winrt.Windows.Storage.Streams;
import winrt.Windows.Web.Http;
import winrt.Microsoft.Windows.Storage;

namespace OpenNet::Core::RSS
{
	using namespace winrt;
	using namespace winrt::Windows::Storage;
	using namespace winrt::Windows::Storage::Streams;
	using namespace winrt::Windows::Web::Http;

	static std::wstring TorrentCacheName(std::wstring const& source)
	{
		std::wstring name = RSSLinkResolver::ExtractFilename(source);
		if (name.empty() || name.find(L'=') != std::wstring::npos)
		{
			std::wregex hashPattern(LR"(([0-9A-Fa-f]{40}))");
			std::wsmatch match;
			name = std::regex_search(source, match, hashPattern)
				? match.str(1)
				: std::format(L"rss-{:016x}", std::hash<std::wstring>{}(source));
		}
		for (auto& ch : name)
		{
			if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
				ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
			{
				ch = L'_';
			}
		}
		if (!name.ends_with(L".torrent"))
		{
			name += L".torrent";
		}
		return name;
	}
    std::wstring RSSLinkResolver::NormalizeFeedUrl(const std::wstring& url)
    {
        if (url.empty())
        {
            return url;
        }

        // Check if already has protocol
        if (url.find(L"http://") == 0 || url.find(L"https://") == 0)
        {
            return url;
        }

        // Check if it looks like a URL (contains . or :)
        if (url.find(L".") != std::wstring::npos || url.find(L":") != std::wstring::npos)
        {
            return L"https://" + url;
        }

        return url;
    }

    std::wstring RSSLinkResolver::NormalizeContentUrl(const std::wstring& url)
    {
        if (url.empty())
        {
            return url;
        }

        // Check if already has protocol
        if (url.find(L"magnet:") == 0)
        {
            return url;  // Magnet links don't need protocol adjustment
        }

        if (url.find(L"http://") == 0 || url.find(L"https://") == 0)
        {
            return url;
        }

        // Check if it looks like a URL
        if (url.find(L".") != std::wstring::npos)
        {
            return L"https://" + url;
        }

        return url;
    }

    bool RSSLinkResolver::IsMagnetLink(const std::wstring& url)
    {
        return url.find(L"magnet:") == 0;
    }

    bool RSSLinkResolver::IsTorrentFileUrl(const std::wstring& url)
    {
        // Check if URL ends with .torrent
        std::wstring lowerUrl = url;
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), std::towlower);
        
        if (lowerUrl.find(L".torrent") != std::wstring::npos)
        {
            return true;
        }

        // Also check for common torrent file indicators
        if (lowerUrl.find(L"/download") != std::wstring::npos ||
            lowerUrl.find(L"/torrent") != std::wstring::npos)
        {
            return true;
        }

        return false;
    }

    std::wstring RSSLinkResolver::ExtractFilename(const std::wstring& url)
    {
        // Find the last / and extract everything after it
        size_t lastSlash = url.find_last_of(L"/\\");
        if (lastSlash != std::wstring::npos)
        {
            std::wstring filename = url.substr(lastSlash + 1);
            
            // Remove query parameters
            size_t queryPos = filename.find(L"?");
            if (queryPos != std::wstring::npos)
            {
                filename = filename.substr(0, queryPos);
            }

            return filename;
        }

        return url;
    }

    bool RSSLinkResolver::IsValidUrl(const std::wstring& url)
    {
        if (url.empty())
        {
            return false;
        }

        std::wstring normalized = NormalizeContentUrl(url);

        // Check if it starts with valid protocol
        if (normalized.find(L"http://") != 0 && 
            normalized.find(L"https://") != 0 &&
            normalized.find(L"magnet:") != 0)
        {
            return false;
        }

        // Basic URL format validation
        if (normalized.length() < 10)  // Minimum valid URL length
        {
            return false;
        }

        return true;
    }

	std::wstring RSSLinkResolver::GetContentType(const std::wstring& url)
    {
        std::wstring lowerUrl = url;
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), std::towlower);

        if (lowerUrl.find(L"magnet:") == 0)
        {
            return L"magnet";
        }

        if (lowerUrl.find(L".torrent") != std::wstring::npos)
        {
            return L"torrent";
        }

        if (lowerUrl.find(L".xml") != std::wstring::npos)
        {
            return L"rss";
        }

        if (lowerUrl.find(L".json") != std::wstring::npos)
        {
            return L"json";
        }

        // Default to HTTP download
        if (lowerUrl.find(L"http://") == 0 || lowerUrl.find(L"https://") == 0)
        {
            return L"http";
        }

		return L"unknown";
	}

	Windows::Foundation::IAsyncOperation<hstring>
		RSSLinkResolver::ResolveTorrentSourceAsync(hstring const& source)
	{
		std::wstring value{ source.c_str() };
		if (value.empty())
		{
			throw hresult_invalid_argument(L"The RSS item has no torrent source.");
		}

		std::wstring lower = value;
		std::transform(lower.begin(), lower.end(), lower.begin(), std::towlower);
		if (lower.starts_with(L"magnet:"))
		{
			co_return source;
		}
		if (!lower.starts_with(L"http://") && !lower.starts_with(L"https://"))
		{
			co_return source;
		}

		HttpClient client;
		client.DefaultRequestHeaders().UserAgent().ParseAdd(L"OpenNet/1.0 RSS Reader");
		auto response = co_await client.GetAsync(Windows::Foundation::Uri(source));
		response.EnsureSuccessStatusCode();
		auto buffer = co_await response.Content().ReadAsBufferAsync();
		if (!buffer || buffer.Length() < 4)
		{
			throw hresult_error(
				winrt::hresult{ -2147467259 },
				L"The torrent endpoint returned an empty response.");
		}

		DataReader reader = DataReader::FromBuffer(buffer);
		if (reader.ReadByte() != static_cast<uint8_t>('d'))
		{
			throw hresult_error(
				winrt::hresult{ -2147467259 },
				L"The RSS enclosure is not a valid bencoded torrent file.");
		}

		// Windows.Storage.ApplicationData::Current can fail with E_ACCESSDENIED
		// for an unpackaged WinUI 3 process. Use the Windows App SDK storage
		// implementation, which is also used by the rest of OpenNet.
		auto folder = winrt::Microsoft::Windows::Storage::ApplicationData::
			GetDefault().TemporaryFolder();
		auto file = co_await folder.CreateFileAsync(
			TorrentCacheName(value), CreationCollisionOption::ReplaceExisting);
		co_await FileIO::WriteBufferAsync(file, buffer);
		co_return file.Path();
	}
}
