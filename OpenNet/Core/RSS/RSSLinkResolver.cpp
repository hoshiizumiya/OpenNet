#include "pch.h"
#include "RSSLinkResolver.h"
#include <algorithm>
#include <cctype>

namespace OpenNet::Core::RSS
{
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
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
        
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
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);

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
}
