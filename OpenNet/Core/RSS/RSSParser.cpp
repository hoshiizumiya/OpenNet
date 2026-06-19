module OpenNet.Core.RSS.RSSParser;

import std;
import winrt.Windows.Data.Xml.Dom;

namespace OpenNet::Core::RSS
{
	using namespace winrt::Windows::Data::Xml::Dom;

	// Helper to register common torrent RSS namespaces
	static void RegisterTorrentNamespaces(XmlDocument const& doc)
	{
		try
		{
			// Get the root element to check for namespace declarations
			auto root = doc.DocumentElement();
			if (!root) return;

			// Common torrent RSS namespaces - we register them so XPath queries work
			// Note: Windows.Data.Xml.Dom doesn't have CreateNamespaceManager(),
			// but we can use SelectNodesNS with inline namespace declarations
		}
		catch (...)
		{
		}
	}

	// Helper to safely select a node with namespace fallback
	static IXmlNode SelectNodeSafe(IXmlNode const& parent, winrt::hstring const& xpath)
	{
		try
		{
			return parent.SelectSingleNode(xpath);
		}
		catch (...)
		{
			return nullptr;
		}
	}

	// Forward declaration
	static bool IsProbableTorrentDownloadUrl(const std::wstring& url);

	// Helper to extract all torrent links from an item node
	static std::vector<std::wstring> ExtractAllTorrentLinks(IXmlNode const& itemNode)
	{
		std::vector<std::wstring> links;

		// Check all enclosure elements
		try
		{
			auto enclosures = itemNode.SelectNodes(L"enclosure");
			for (uint32_t i = 0; i < enclosures.Size(); ++i)
			{
				auto enclosure = enclosures.GetAt(i).try_as<XmlElement>();
				if (enclosure)
				{
					auto url = enclosure.GetAttribute(L"url");
					auto type = enclosure.GetAttribute(L"type");

					// Check if it's a torrent-related enclosure
					std::wstring typeStr = type.c_str();
					std::wstring urlStr = url.c_str();

					if (typeStr.find(L"bittorrent") != std::wstring::npos ||
						typeStr.find(L"torrent") != std::wstring::npos ||
						urlStr.find(L".torrent") != std::wstring::npos ||
						urlStr.find(L"magnet:") == 0 ||
						urlStr.find(L"down") != std::wstring::npos ||
						IsProbableTorrentDownloadUrl(urlStr))
					{
						if (!urlStr.empty())
						{
							links.push_back(urlStr);
						}
					}
				}
			}
		}
		catch (...)
		{
		}

		// Check link elements (for magnet links)
		try
		{
			auto linkNode = itemNode.SelectSingleNode(L"link");
			if (linkNode)
			{
				std::wstring linkText = linkNode.InnerText().c_str();
				if (linkText.find(L"magnet:") == 0 || linkText.find(L".torrent") != std::wstring::npos
					|| IsProbableTorrentDownloadUrl(linkText))
				{
					links.push_back(linkText);
				}
			}
		}
		catch (...)
		{
		}

		// Try torrent namespace elements with namespace prefix in XPath
		// Note: SelectSingleNodeNS is not available, so we try common patterns
		const wchar_t* torrentXPaths[] = {
			L"*[local-name()='magnetURI']",
			L"*[local-name()='infoHash']",
			L"*[local-name()='contentLength']"
		};

		for (const auto& xpath : torrentXPaths)
		{
			try
			{
				auto node = itemNode.SelectSingleNode(xpath);
				if (node)
				{
					std::wstring text = node.InnerText().c_str();
					if (text.find(L"magnet:") == 0)
					{
						links.push_back(text);
					}
				}
			}
			catch (...)
			{
			}
		}

		return links;
	}

	// Helper: is the URL likely a torrent download link?
	// Recognizes URLs like:
	//   https://tracker.example.com/download.php?id=123&passkey=abc
	//   https://example.com/dl/abc123def456.torrent
	//   https://rss.example.com/torrents/download/HASH
	// Patterns inspired by qBittorrent's RSS torrent-link heuristics.
	static bool IsProbableTorrentDownloadUrl(const std::wstring& url)
	{
		// Already handled upstream: magnet: prefix and .torrent suffix
		// Here we check for other common torrent-tracker download patterns.

		// URL path/query patterns that suggest a torrent download endpoint
		static const std::vector<std::wstring> downloadPatterns = {
			L"download.php",
			L"download?",
			L"/download/",
			L"/dl/",
			L"action=download",
			L"passkey=",
			L"authkey=",
			L"torrent_pass=",
			L"tp=",
			L"/rss/download/",
		};

		std::wstring lower = url;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

		for (const auto& pattern : downloadPatterns)
		{
			if (lower.find(pattern) != std::wstring::npos)
				return true;
		}

		// Check for info-hash-like hex string (40 chars) in the URL
		static const std::wregex hashPattern(L"[0-9a-fA-F]{40}");
		if (std::regex_search(url, hashPattern))
			return true;

		return false;
	}

	// Select the best torrent link (prefer magnet, then .torrent, then download URL)
	static std::wstring SelectBestTorrentLink(const std::vector<std::wstring>& links)
	{
		if (links.empty()) return L"";

		// First, prefer magnet links
		for (const auto& link : links)
		{
			if (link.find(L"magnet:") == 0)
			{
				return link;
			}
		}

		// Then prefer .torrent files
		for (const auto& link : links)
		{
			if (link.find(L".torrent") != std::wstring::npos)
			{
				return link;
			}
		}

		// Then prefer probable torrent download URLs (passkey, hash, etc.)
		for (const auto& link : links)
		{
			if (IsProbableTorrentDownloadUrl(link))
			{
				return link;
			}
		}

		// Return first available
		return links[0];
	}

	std::optional<RSSFeed> RSSParser::Parse(const std::wstring& xmlContent, const std::wstring& feedUrl)
	{
		try
		{
			XmlDocument doc;
			doc.LoadXml(xmlContent);

			RSSFeed feed;
			feed.url = feedUrl;

			// Register common namespaces
			RegisterTorrentNamespaces(doc);

			// Try RSS 2.0 format first
			auto channelNodes = doc.SelectNodes(L"//channel");
			if (channelNodes.Size() > 0)
			{
				auto channel = channelNodes.GetAt(0);

				// Get channel info
				auto titleNode = SelectNodeSafe(channel, L"title");
				if (titleNode)
				{
					feed.title = titleNode.InnerText().c_str();
				}

				auto descNode = SelectNodeSafe(channel, L"description");
				if (descNode)
				{
					feed.description = descNode.InnerText().c_str();
				}

				// Parse items
				auto itemNodes = channel.SelectNodes(L"item");
				for (uint32_t i = 0; i < itemNodes.Size(); ++i)
				{
					auto itemNode = itemNodes.GetAt(i);
					RSSItem item;

					auto itemTitleNode = SelectNodeSafe(itemNode, L"title");
					if (itemTitleNode)
					{
						item.title = itemTitleNode.InnerText().c_str();
					}

					auto linkNode = SelectNodeSafe(itemNode, L"link");
					if (linkNode)
					{
						item.link = linkNode.InnerText().c_str();
					}

					auto descItemNode = SelectNodeSafe(itemNode, L"description");
					if (descItemNode)
					{
						item.description = descItemNode.InnerText().c_str();
					}

					auto guidNode = SelectNodeSafe(itemNode, L"guid");
					if (guidNode)
					{
						item.guid = guidNode.InnerText().c_str();
					}

					auto pubDateNode = SelectNodeSafe(itemNode, L"pubDate");
					if (pubDateNode)
					{
						item.pubDate = ParsePubDate(pubDateNode.InnerText().c_str());
					}

					auto categoryNode = SelectNodeSafe(itemNode, L"category");
					if (categoryNode)
					{
						item.category = categoryNode.InnerText().c_str();
					}

					// Extract all torrent links and select the best one
					auto torrentLinks = ExtractAllTorrentLinks(itemNode);
					item.enclosureUrl = SelectBestTorrentLink(torrentLinks);

					// Also get enclosure metadata (size, type) from first enclosure
					try
					{
						auto enclosureNode = itemNode.SelectSingleNode(L"enclosure");
						if (enclosureNode)
						{
							auto element = enclosureNode.try_as<XmlElement>();
							if (element)
							{
								if (item.enclosureUrl.empty())
								{
									item.enclosureUrl = element.GetAttribute(L"url").c_str();
								}
								item.enclosureType = element.GetAttribute(L"type").c_str();
								auto lengthAttr = element.GetAttribute(L"length");
								if (!lengthAttr.empty())
								{
									try
									{
										item.enclosureLength = std::stoull(std::wstring(lengthAttr.c_str()));
									}
									catch (...)
									{
									}
								}
							}
						}
					}
					catch (...)
					{
					}

					feed.items.push_back(std::move(item));
				}

				return feed;
			}

			// Try Atom format
			auto entryNodes = doc.SelectNodes(L"//entry");
			if (entryNodes.Size() > 0)
			{
				auto titleNode = SelectNodeSafe(doc, L"//feed/title");
				if (titleNode)
				{
					feed.title = titleNode.InnerText().c_str();
				}

				for (uint32_t i = 0; i < entryNodes.Size(); ++i)
				{
					auto entryNode = entryNodes.GetAt(i);
					RSSItem item;

					auto itemTitleNode = SelectNodeSafe(entryNode, L"title");
					if (itemTitleNode)
					{
						item.title = itemTitleNode.InnerText().c_str();
					}

					try
					{
						auto linkNodes = entryNode.SelectNodes(L"link");
						for (uint32_t j = 0; j < linkNodes.Size(); ++j)
						{
							auto linkNode = linkNodes.GetAt(j).try_as<XmlElement>();
							if (linkNode)
							{
								auto rel = linkNode.GetAttribute(L"rel");
								auto href = linkNode.GetAttribute(L"href");
								auto type = linkNode.GetAttribute(L"type");

								std::wstring hrefStr = href.c_str();
								std::wstring typeStr = type.c_str();

								if (typeStr.find(L"bittorrent") != std::wstring::npos ||
									hrefStr.find(L"magnet:") == 0 ||
									hrefStr.find(L".torrent") != std::wstring::npos)
								{
									item.enclosureUrl = hrefStr;
									item.enclosureType = typeStr;
								}
								else if (rel.empty() || rel == L"alternate")
								{
									item.link = hrefStr;
								}
							}
						}
					}
					catch (...)
					{
					}

					auto idNode = SelectNodeSafe(entryNode, L"id");
					if (idNode)
					{
						item.guid = idNode.InnerText().c_str();
					}

					auto updatedNode = SelectNodeSafe(entryNode, L"updated");
					if (updatedNode)
					{
						item.pubDate = ParsePubDate(updatedNode.InnerText().c_str());
					}

					feed.items.push_back(std::move(item));
				}

				return feed;
			}

			return std::nullopt;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::wstring RSSParser::ExtractTorrentLink(const RSSItem& item)
	{
		// Priority: enclosure URL > link (if it's a torrent/magnet/download URL)
		if (!item.enclosureUrl.empty())
		{
			return item.enclosureUrl;
		}

		// Check if link is a torrent file or magnet link
		if (item.link.find(L".torrent") != std::wstring::npos ||
			item.link.find(L"magnet:") == 0)
		{
			return item.link;
		}

		// Check if link looks like a torrent download URL (passkey, hash, etc.)
		if (!item.link.empty() && IsProbableTorrentDownloadUrl(item.link))
		{
			return item.link;
		}

		return L"";
	}

	bool RSSParser::MatchesFilter(const RSSItem& item, const std::wstring& filterPattern)
	{
		if (filterPattern.empty())
		{
			return true;  // No filter means match all
		}

		try
		{
			std::wregex pattern(filterPattern, std::regex_constants::icase);
			return std::regex_search(item.title, pattern) ||
				std::regex_search(item.description, pattern);
		}
		catch (const std::regex_error&)
		{
			// Invalid regex, fall back to simple substring match
			std::wstring lowerTitle = item.title;
			std::wstring lowerFilter = filterPattern;
			std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::towlower);
			std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::towlower);
			return lowerTitle.find(lowerFilter) != std::wstring::npos;
		}
	}

	std::chrono::system_clock::time_point RSSParser::ParsePubDate(const std::wstring& dateStr)
	{
		// Parse RFC 2822 date format: "Fri, 27 Feb 2026 14:15:04 +0800"
		// Also handles: "2026-02-27T14:15:04Z" (ISO 8601)
		try
		{
			auto utf8 = winrt::to_string(dateStr);
			{
				std::chrono::sys_seconds tp;
				std::chrono::minutes offset;

				std::istringstream ss(utf8);

				ss >> std::chrono::parse(
					"%a, %d %b %Y %T %z",
					tp,
					offset
				);

				if (!ss.fail())
					return tp;
			}

			{
				std::chrono::sys_seconds tp;
				std::chrono::minutes offset;

				std::istringstream ss(utf8);

				ss >> std::chrono::parse(
					"%FT%T%Ez",
					tp,
					offset
				);

				if (!ss.fail())
					return tp;
			}

			{
				std::chrono::sys_seconds tp;

				std::istringstream ss(utf8);

				ss >> std::chrono::parse(
					"%FT%TZ",
					tp
				);

				if (!ss.fail())
					return tp;
			}
		}
		catch (...)
		{
		}

		return std::chrono::system_clock::now();
	}
}
