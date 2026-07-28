#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "WebUIHost.h"

#include <Windows.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/version.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Core/IPFilter/IPFilterManager.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.GeoIP.GeoIPManager;
import OpenNet.Core.P2PManager;
import OpenNet.Core.RSS.RSSManager;
import OpenNet.Core.RSS.RSSTypes;
import OpenNet.Core.TorrentSettings;
import OpenNet.Core.torrentCore.LibtorrentHandle;
import OpenNet.Core.torrentCore.TorrentMetadataFetcher;
import OpenNet.Core.torrentCore.TorrentMetadataInfo;
import OpenNet.Core.torrentCore.TorrentStateManager;

namespace OpenNet::Core::WebUI
{
	namespace asio = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;
	using tcp = asio::ip::tcp;
	using Json = nlohmann::json;
	using Request = http::request<http::string_body>;
	using Response = http::response<http::string_body>;

	namespace
	{
		constexpr std::size_t MaxRequestBody = 32U * 1024U * 1024U;
		constexpr std::size_t MaxStaticFile = 10U * 1024U * 1024U;
		constexpr auto SessionLifetime = std::chrono::hours(1);
		constexpr std::string_view ApiKeySession = "api-key";

		struct FormPart
		{
			std::string name;
			std::string filename;
			std::string data;
		};

		std::string ToLower(std::string value)
		{
			std::ranges::transform(value, value.begin(), [](const unsigned char ch)
			{
				return static_cast<char>(std::tolower(ch));
			});
			return value;
		}

		std::string Trim(std::string_view value)
		{
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
				value.remove_prefix(1);
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
				value.remove_suffix(1);
			return std::string(value);
		}

		std::optional<std::int64_t> ParseInt64(
			const std::string_view value)
		{
			std::int64_t result{};
			const auto [position, error] = std::from_chars(
				value.data(), value.data() + value.size(), result);
			if (error != std::errc{}
				|| position != value.data() + value.size())
			{
				return std::nullopt;
			}
			return result;
		}

		bool ParseBool(const std::string_view value)
		{
			const std::string normalized = ToLower(Trim(value));
			return normalized == "true" || normalized == "1"
				|| normalized == "yes" || normalized == "on";
		}

		std::vector<std::string> SplitValues(
			std::string_view value, const char separator)
		{
			std::vector<std::string> result;
			while (!value.empty())
			{
				const auto position = value.find(separator);
				auto item = Trim(value.substr(0, position));
				if (!item.empty())
					result.push_back(std::move(item));
				if (position == std::string_view::npos)
					break;
				value.remove_prefix(position + 1);
			}
			return result;
		}

		std::optional<bool> ParseBoolean(std::string_view value)
		{
			const auto lower = ToLower(Trim(value));
			if (lower == "true" || lower == "1")
				return true;
			if (lower == "false" || lower == "0")
				return false;
			return std::nullopt;
		}

		bool ConstantTimeEquals(std::string_view left, std::string_view right)
		{
			std::size_t difference = left.size() ^ right.size();
			const std::size_t count = std::max(left.size(), right.size());
			for (std::size_t i = 0; i < count; ++i)
			{
				const unsigned char leftValue = (i < left.size())
					? static_cast<unsigned char>(left[i])
					: 0;
				const unsigned char rightValue = (i < right.size())
					? static_cast<unsigned char>(right[i])
					: 0;
				difference |= leftValue ^ rightValue;
			}
			return difference == 0;
		}

		std::string RandomHex(const std::size_t byteCount)
		{
			std::random_device random;
			static constexpr char Hex[] = "0123456789abcdef";
			std::string result(byteCount * 2, '0');
			for (std::size_t i = 0; i < byteCount; ++i)
			{
				const auto value = static_cast<unsigned char>(random());
				result[i * 2] = Hex[value >> 4];
				result[(i * 2) + 1] = Hex[value & 0x0f];
			}
			return result;
		}

		std::optional<std::string> PercentDecode(
			std::string_view input, const bool plusAsSpace)
		{
			std::string result;
			result.reserve(input.size());
			for (std::size_t i = 0; i < input.size(); ++i)
			{
				const char ch = input[i];
				if (plusAsSpace && ch == '+')
				{
					result.push_back(' ');
					continue;
				}
				if (ch != '%')
				{
					if (ch == '\0')
						return std::nullopt;
					result.push_back(ch);
					continue;
				}
				if ((i + 2) >= input.size())
					return std::nullopt;
				unsigned int value = 0;
				const auto hex = input.substr(i + 1, 2);
				const auto [ptr, error] = std::from_chars(
					hex.data(), hex.data() + hex.size(), value, 16);
				if ((error != std::errc{}) || (ptr != (hex.data() + hex.size())))
					return std::nullopt;
				const char decoded = static_cast<char>(value);
				if (decoded == '\0')
					return std::nullopt;
				result.push_back(decoded);
				i += 2;
			}
			return result;
		}

		std::unordered_map<std::string, std::string> ParseKeyValue(
			std::string_view value, const bool plusAsSpace = true)
		{
			std::unordered_map<std::string, std::string> result;
			while (!value.empty())
			{
				const auto separator = value.find('&');
				const auto item = value.substr(0, separator);
				const auto equals = item.find('=');
				const auto key = PercentDecode(item.substr(0, equals), plusAsSpace);
				const auto itemValue = (equals == std::string_view::npos)
					? std::optional<std::string>{std::string{}}
				: PercentDecode(item.substr(equals + 1), plusAsSpace);
				if (key && itemValue)
					result[*key] = *itemValue;

				if (separator == std::string_view::npos)
					break;
				value.remove_prefix(separator + 1);
			}
			return result;
		}

		std::string HeaderParameter(
			std::string_view header, std::string_view parameter)
		{
			const std::string lowerHeader = ToLower(std::string(header));
			const std::string token = ToLower(std::string(parameter)) + "=";
			auto position = lowerHeader.find(token);
			if (position == std::string::npos)
				return {};
			position += token.size();
			if (position >= header.size())
				return {};

			if (header[position] == '"')
			{
				const auto end = header.find('"', position + 1);
				if (end == std::string_view::npos)
					return {};
				return std::string(header.substr(position + 1, end - position - 1));
			}

			const auto end = header.find(';', position);
			return Trim(header.substr(position, end - position));
		}

		std::vector<FormPart> ParseMultipart(
			std::string_view body, std::string_view contentType)
		{
			const std::string boundaryValue = HeaderParameter(contentType, "boundary");
			if (boundaryValue.empty() || boundaryValue.size() > 200)
				return {};

			const std::string boundary = "--" + boundaryValue;
			const std::string delimiter = "\r\n" + boundary;
			std::vector<FormPart> result;
			std::size_t position = body.find(boundary);
			while (position != std::string_view::npos)
			{
				position += boundary.size();
				if (body.substr(position, 2) == "--")
					break;
				if (body.substr(position, 2) != "\r\n")
					return {};
				position += 2;

				const auto headerEnd = body.find("\r\n\r\n", position);
				if (headerEnd == std::string_view::npos)
					return {};
				const auto headerBlock = body.substr(position, headerEnd - position);
				const auto dataStart = headerEnd + 4;
				const auto nextBoundary = body.find(delimiter, dataStart);
				if (nextBoundary == std::string_view::npos)
					return {};

				FormPart part;
				std::size_t headerPosition = 0;
				while (headerPosition < headerBlock.size())
				{
					const auto lineEnd = headerBlock.find("\r\n", headerPosition);
					const auto line = headerBlock.substr(
						headerPosition, lineEnd - headerPosition);
					const auto colon = line.find(':');
					if (colon != std::string_view::npos)
					{
						const auto name = ToLower(Trim(line.substr(0, colon)));
						const auto headerValue = Trim(line.substr(colon + 1));
						if (name == "content-disposition")
						{
							part.name = HeaderParameter(headerValue, "name");
							part.filename = HeaderParameter(headerValue, "filename");
						}
					}
					if (lineEnd == std::string_view::npos)
						break;
					headerPosition = lineEnd + 2;
				}

				part.data.assign(body.substr(dataStart, nextBoundary - dataStart));
				if (!part.name.empty())
					result.push_back(std::move(part));
				position = nextBoundary + 2;
			}
			return result;
		}

		std::unordered_map<std::string, std::string> ParseCookie(
			std::string_view value)
		{
			std::unordered_map<std::string, std::string> result;
			while (!value.empty())
			{
				const auto separator = value.find(';');
				const auto item = value.substr(0, separator);
				const auto equals = item.find('=');
				if (equals != std::string_view::npos)
					result[Trim(item.substr(0, equals))] = Trim(item.substr(equals + 1));
				if (separator == std::string_view::npos)
					break;
				value.remove_prefix(separator + 1);
			}
			return result;
		}

		std::pair<std::string, std::string> SplitTarget(
			const beast::string_view target)
		{
			const std::string value(target);
			const auto separator = value.find('?');
			if (separator == std::string::npos)
				return { value, {} };
			return { value.substr(0, separator), value.substr(separator + 1) };
		}

		Response BaseResponse(
			const Request& request, const http::status status)
		{
			Response response{ status, request.version() };
			response.keep_alive(request.keep_alive());
			response.set(http::field::server, "OpenNet/qBittorrent-WebAPI");
			response.set("X-Content-Type-Options", "nosniff");
			response.set("Cross-Origin-Opener-Policy", "same-origin");
			response.set("Referrer-Policy", "same-origin");
			response.set("X-Frame-Options", "SAMEORIGIN");
			response.set("Content-Security-Policy",
						 "default-src 'self'; style-src 'self' 'unsafe-inline'; "
						 "img-src 'self' data:; script-src 'self' 'unsafe-inline'; "
						 "object-src 'none'; form-action 'self'; frame-src 'self' blob:; "
						 "frame-ancestors 'self'");
			return response;
		}

		Response TextResponse(
			const Request& request, const http::status status,
			std::string body, std::string_view contentType = "text/plain; charset=UTF-8")
		{
			auto response = BaseResponse(request, status);
			response.set(http::field::content_type, contentType);
			response.body() = std::move(body);
			response.prepare_payload();
			if (request.method() == http::verb::head)
			{
				const auto size = response.body().size();
				response.body().clear();
				response.content_length(size);
			}
			return response;
		}

		Response JsonResponse(
			const Request& request, const Json& value,
			const http::status status = http::status::ok)
		{
			return TextResponse(
				request, status, value.dump(), "application/json; charset=UTF-8");
		}

		Response EmptyResponse(
			const Request& request,
			const http::status status = http::status::ok)
		{
			auto response = BaseResponse(request, status);
			response.prepare_payload();
			return response;
		}

		Response BinaryResponse(
			const Request& request, std::string body,
			const std::string_view contentType,
			const std::string_view fileName = {})
		{
			auto response = TextResponse(
				request, http::status::ok, std::move(body), contentType);
			if (!fileName.empty())
			{
				response.set(
					http::field::content_disposition,
					"attachment; filename=\"" + std::string(fileName) + "\"");
			}
			return response;
		}

		std::string MimeType(const std::filesystem::path& path)
		{
			const std::string extension = ToLower(path.extension().string());
			if (extension == ".htm" || extension == ".html")
				return "text/html; charset=UTF-8";
			if (extension == ".css")
				return "text/css; charset=UTF-8";
			if (extension == ".js" || extension == ".mjs")
				return "text/javascript; charset=UTF-8";
			if (extension == ".json" || extension == ".map")
				return "application/json; charset=UTF-8";
			if (extension == ".svg")
				return "image/svg+xml";
			if (extension == ".png")
				return "image/png";
			if (extension == ".gif")
				return "image/gif";
			if (extension == ".jpg" || extension == ".jpeg")
				return "image/jpeg";
			if (extension == ".ico")
				return "image/x-icon";
			if (extension == ".woff")
				return "font/woff";
			if (extension == ".woff2")
				return "font/woff2";
			if (extension == ".ttf")
				return "font/ttf";
			if (extension == ".txt")
				return "text/plain; charset=UTF-8";
			return "application/octet-stream";
		}

		std::string PathUtf8(const std::filesystem::path& path)
		{
			const std::u8string value = path.u8string();
			return {
				reinterpret_cast<const char*>(value.data()),
				value.size()
			};
		}

		std::filesystem::path PathFromUtf8(const std::string_view value)
		{
			return std::filesystem::path(std::u8string(
				reinterpret_cast<const char8_t*>(value.data()),
				value.size()));
		}

		std::wstring Utf8ToWide(const std::string_view value)
		{
			if (value.empty())
				return {};
			const int required = MultiByteToWideChar(
				CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
				static_cast<int>(value.size()), nullptr, 0);
			if (required <= 0)
				throw std::invalid_argument("Invalid UTF-8 text");
			std::wstring result(static_cast<std::size_t>(required), L'\0');
			MultiByteToWideChar(
				CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
				static_cast<int>(value.size()), result.data(), required);
			return result;
		}

		std::string WideToUtf8(const std::wstring_view value)
		{
			if (value.empty())
				return {};
			const int required = WideCharToMultiByte(
				CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
				static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
			if (required <= 0)
				throw std::invalid_argument("Invalid Unicode text");
			std::string result(static_cast<std::size_t>(required), '\0');
			WideCharToMultiByte(
				CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
				static_cast<int>(value.size()), result.data(), required,
				nullptr, nullptr);
			return result;
		}

		Json SerializeMetadata(
			const ::OpenNet::Core::Torrent::TorrentMetadataInfo& metadata)
		{
			Json files = Json::array();
			for (const auto& file : metadata.files)
			{
				files.push_back(Json{
					{"index", file.fileIndex},
					{"name", file.path},
					{"size", file.size}
								});
			}
			return Json{
				{"infohash_v1", metadata.infoHash},
				{"infohash_v2", ""},
				{"name", metadata.name},
				{"comment", metadata.comment},
				{"created_by", metadata.creator},
				{"creation_date", metadata.creationDate},
				{"piece_size", metadata.pieceLength},
				{"pieces", metadata.numPieces},
				{"total_size", metadata.totalSize},
				{"private", metadata.isPrivate},
				{"files", std::move(files)},
				{"trackers", metadata.trackers},
				{"url_seeds", metadata.webSeeds}
			};
		}

		bool IsTextAsset(const std::filesystem::path& path)
		{
			const std::string extension = ToLower(path.extension().string());
			return extension == ".html" || extension == ".htm"
				|| extension == ".css" || extension == ".js"
				|| extension == ".mjs" || extension == ".txt"
				|| extension == ".svg";
		}

		bool IsPathInside(
			const std::filesystem::path& root,
			const std::filesystem::path& candidate)
		{
			const auto normalizedRoot = std::filesystem::weakly_canonical(root);
			const auto normalizedCandidate = std::filesystem::weakly_canonical(candidate);
			auto rootIterator = normalizedRoot.begin();
			auto candidateIterator = normalizedCandidate.begin();
			for (; rootIterator != normalizedRoot.end(); ++rootIterator, ++candidateIterator)
			{
				if (candidateIterator == normalizedCandidate.end())
					return false;
#ifdef _WIN32
				if (ToLower(rootIterator->string()) != ToLower(candidateIterator->string()))
					return false;
#else
				if (*rootIterator != *candidateIterator)
					return false;
#endif
			}
			return true;
		}

		std::filesystem::path ExecutableDirectory()
		{
			std::wstring value(32768, L'\0');
			const DWORD length = GetModuleFileNameW(
				nullptr, value.data(), static_cast<DWORD>(value.size()));
			if (length == 0 || length >= value.size())
				return {};
			value.resize(length);
			return std::filesystem::path(value).parent_path();
		}

		std::filesystem::path ResolveAssetRoot(
			const std::filesystem::path& requested)
		{
			std::vector<std::filesystem::path> candidates;
			if (!requested.empty())
				candidates.push_back(requested);
			const auto executable = ExecutableDirectory();
			if (!executable.empty())
			{
				candidates.push_back(executable / L"WebUI");
				candidates.push_back(
					executable / L"ThirdParty" / L"qBittorrentWebUI" / L"upstream");
			}
			const auto current = std::filesystem::current_path();
			candidates.push_back(
				current / L"OpenNet" / L"ThirdParty" / L"qBittorrentWebUI" / L"upstream");
			candidates.push_back(
				current / L"ThirdParty" / L"qBittorrentWebUI" / L"upstream");

			for (const auto& candidate : candidates)
			{
				std::error_code error;
				if (std::filesystem::is_regular_file(
					candidate / L"private" / L"index.html", error)
					&& std::filesystem::is_regular_file(
						candidate / L"public" / L"index.html", error))
				{
					return std::filesystem::weakly_canonical(candidate);
				}
			}
			return {};
		}

		std::string ReplaceAll(
			std::string value, std::string_view token, std::string_view replacement)
		{
			std::size_t position = 0;
			while ((position = value.find(token, position)) != std::string::npos)
			{
				value.replace(position, token.size(), replacement);
				position += replacement.size();
			}
			return value;
		}

		std::string StripTranslationMarkers(std::string value)
		{
			constexpr std::string_view Prefix = "QBT_TR(";
			constexpr std::string_view Context = ")QBT_TR[CONTEXT=";
			std::size_t cursor = 0;
			while ((cursor = value.find(Prefix, cursor)) != std::string::npos)
			{
				const std::size_t textStart = cursor + Prefix.size();
				const std::size_t contextStart = value.find(Context, textStart);
				if (contextStart == std::string::npos)
					break;
				const std::size_t markerEnd = value.find(']', contextStart + Context.size());
				if (markerEnd == std::string::npos)
					break;
				const std::string translated = value.substr(
					textStart, contextStart - textStart);
				value.replace(cursor, markerEnd + 1 - cursor, translated);
				cursor += translated.size();
			}
			return value;
		}

		std::string LanguageOptions(const std::filesystem::path& assetRoot)
		{
			std::vector<std::string> languages;
			std::error_code error;
			const auto translations = assetRoot / L"translations";
			for (const auto& entry : std::filesystem::directory_iterator(
				translations,
				std::filesystem::directory_options::skip_permission_denied,
				error))
			{
				if (!entry.is_regular_file())
					continue;
				const std::string filename = entry.path().filename().string();
				if (!filename.starts_with("webui_") || !filename.ends_with(".ts"))
					continue;
				languages.push_back(filename.substr(6, filename.size() - 9));
			}
			std::ranges::sort(languages);
			std::string result;
			for (const auto& language : languages)
			{
				result += "<option value=\"" + language + "\">"
					+ language + "</option>\n";
			}
			return result;
		}

		std::string DefaultSavePath()
		{
			const auto configured =
				::OpenNet::Core::TorrentSettingsManager::Instance().Get().defaultSavePath;
			if (!configured.empty())
				return PathUtf8(std::filesystem::path(configured));

			const DWORD required = GetEnvironmentVariableW(
				L"USERPROFILE", nullptr, 0);
			if (required > 1)
			{
				std::wstring userProfile(required, L'\0');
				const DWORD length = GetEnvironmentVariableW(
					L"USERPROFILE", userProfile.data(), required);
				if (length == 0 || length >= required)
					return PathUtf8(std::filesystem::current_path());
				userProfile.resize(length);
				return PathUtf8(
					std::filesystem::path(userProfile) / L"Downloads");
			}
			return PathUtf8(std::filesystem::current_path());
		}

		std::string QbtState(
			const ::OpenNet::Core::Torrent::LibtorrentHandle::TorrentDetailInfo& detail)
		{
			const bool complete = detail.totalSize > 0
				&& detail.totalDone >= detail.totalSize;
			if (detail.isPaused)
				return complete ? "stoppedUP" : "stoppedDL";

			switch (detail.state)
			{
				case 1:
					return complete ? "checkingUP" : "checkingDL";
				case 2:
					return "metaDL";
				case 3:
					return detail.downloadRate > 0 ? "downloading" : "stalledDL";
				case 4:
				case 5:
					return detail.uploadRate > 0 ? "uploading" : "stalledUP";
				case 7:
					return "checkingResumeData";
				default:
					return "unknown";
			}
		}

		Json SerializeTorrent(
			const ::OpenNet::Core::Torrent::TaskMetadata& task,
			const ::OpenNet::Core::Torrent::LibtorrentHandle::TorrentDetailInfo& detail)
		{
			const std::int64_t totalSize = std::max(detail.totalSize, task.totalSize);
			const std::int64_t completed = std::max(detail.totalDone, task.downloadedSize);
			const double progress = detail.progressPpm > 0
				? (static_cast<double>(detail.progressPpm) / 1'000'000.0)
				: ((totalSize > 0)
				   ? static_cast<double>(completed) / static_cast<double>(totalSize)
				   : 0.0);
			const std::string name = !detail.name.empty() ? detail.name : task.name;
			const std::string savePath = !detail.savePath.empty()
				? detail.savePath : task.savePath;
			const std::string hash = !detail.apiHash.empty()
				? detail.apiHash : task.taskId;
			const bool complete = totalSize > 0 && completed >= totalSize;
			const std::int64_t eta = (detail.downloadRate > 0 && totalSize > completed)
				? ((totalSize - completed) / detail.downloadRate)
				: 86'400 * 100;

			return Json{
				{"hash", hash},
				{"infohash_v1", detail.infoHashV1},
				{"infohash_v2", detail.infoHashV2},
				{"name", name},
				{"magnet_uri", task.magnetUri},
				{"has_metadata", !name.empty() || totalSize > 0},
				{"created_by", ""},
				{"creation_date", -1},
				{"private", nullptr},
				{"total_size", totalSize},
				{"pieces_num", detail.piecesNum},
				{"piece_size", detail.pieceSize},
				{"size", totalSize},
				{"progress", std::clamp(progress, 0.0, 1.0)},
				{"total_wasted", 0},
				{"pieces_have", detail.pieceSize > 0
					? static_cast<std::int64_t>(
						completed / detail.pieceSize)
					: 0},
				{"dlspeed", detail.downloadRate},
				{"upspeed", detail.uploadRate},
				{"priority", detail.queuePosition},
				{"num_seeds", detail.numSeeds},
				{"num_complete", detail.numSeeds},
				{"num_leechs", std::max(0, detail.numPeers - detail.numSeeds)},
				{"num_incomplete", std::max(0, detail.numPeers - detail.numSeeds)},
				{"state", QbtState(detail)},
				{"eta", eta},
				{"seq_dl", detail.isSequential},
				{"f_l_piece_prio",
					detail.firstLastPiecePriority},
				{"category", ""},
				{"tags", ""},
				{"super_seeding", detail.isSuperSeeding},
				{"force_start",
					!detail.isAutoManaged && !detail.isPaused},
				{"save_path", savePath},
				{"download_path", ""},
				{"content_path", savePath},
				{"root_path", savePath},
				{"added_on", task.addedTimestamp},
				{"completion_on", complete ? task.addedTimestamp : -1},
				{"tracker", detail.trackers.empty() ? "" : detail.trackers.front().url},
				{"trackers_count", detail.trackers.size()},
				{"dl_limit", 0},
				{"up_limit", 0},
				{"downloaded", completed},
				{"uploaded", detail.totalUploaded},
				{"downloaded_session", completed},
				{"uploaded_session", detail.totalUploaded},
				{"amount_left", std::max<std::int64_t>(0, totalSize - completed)},
				{"completed", completed},
				{"connections", detail.numConnections},
				{"connections_limit", 0},
				{"max_ratio", -1},
				{"max_seeding_time", -1},
				{"max_inactive_seeding_time", -1},
				{"ratio", detail.shareRatio},
				{"ratio_limit", -2},
				{"popularity", 0.0},
				{"seeding_time_limit", -2},
				{"inactive_seeding_time_limit", -2},
				{"share_limit_mode", "global"},
				{"share_limit_action", "stop"},
				{"last_seen_complete", -1},
				{"auto_tmm", detail.isAutoManaged},
				{"time_active", 0},
				{"seeding_time", 0},
				{"last_activity", task.addedTimestamp},
				{"availability", complete ? 1.0 : 0.0},
				{"reannounce", 0},
				{"comment", detail.comment}
			};
		}

		Json DefaultClientData()
		{
			return Json{
				{"add_new_torrent_dialog_enabled", false},
				{"add_torrent_default_category", ""},
				{"add_torrent_separate_dialog_per_torrent", false},
				{"color_scheme", "auto"},
				{"date_format", "YYYY-MM-DD HH:mm"},
				{"dblclick_complete", "0"},
				{"dblclick_download", "0"},
				{"dblclick_filter", "0"},
				{"display_density", "normal"},
				{"full_url_tracker_column", false},
				{"hide_zero_status_filters", false},
				{"qbt_selected_log_levels", 15},
				{"search_in_filter", false},
				{"show_filters_sidebar", true},
				{"show_log_viewer", false},
				{"show_rss_reader", false},
				{"show_search_engine", false},
				{"show_status_bar", true},
				{"show_top_toolbar", true},
				{"speed_in_browser_title_bar", false},
				{"torrent_creator", false},
				{"use_alt_row_colors", false},
				{"use_virtual_list", true}
			};
		}

		class HttpSession final
			: public std::enable_shared_from_this<HttpSession>
		{
		public:
			using Handler = std::function<Response(
				const Request&, const tcp::endpoint&)>;

			HttpSession(tcp::socket socket, Handler handler)
				: m_socket(std::move(socket))
				, m_handler(std::move(handler))
			{
			}

			void Run()
			{
				asio::dispatch(
					m_socket.get_executor(),
					beast::bind_front_handler(
						&HttpSession::Read, shared_from_this()));
			}

		private:
			void Read()
			{
				m_request = {};
				m_parser.emplace();
				m_parser->body_limit(MaxRequestBody);
				http::async_read(
					m_socket, m_buffer, *m_parser,
					beast::bind_front_handler(
						&HttpSession::OnRead, shared_from_this()));
			}

			void OnRead(const beast::error_code& error, std::size_t)
			{
				if (error == http::error::end_of_stream)
					return Close();
				if (error)
					return;

				m_request = m_parser->release();
				Response response;
				try
				{
					response = m_handler(
						m_request, m_socket.remote_endpoint());
				}
				catch (const std::exception& exception)
				{
					response = TextResponse(
						m_request, http::status::internal_server_error,
						std::string("Internal Server Error: ") + exception.what());
				}
				catch (...)
				{
					response = TextResponse(
						m_request, http::status::internal_server_error,
						"Internal Server Error");
				}

				const bool close = response.need_eof();
				auto message = std::make_shared<Response>(std::move(response));
				http::async_write(
					m_socket, *message,
					[self = shared_from_this(), message, close](
						const beast::error_code& writeError, std::size_t)
				{
					self->OnWrite(writeError, close);
				});
			}

			void OnWrite(const beast::error_code& error, const bool close)
			{
				if (error)
					return;
				if (close)
					return Close();
				Read();
			}

			void Close()
			{
				beast::error_code error;
				m_socket.shutdown(tcp::socket::shutdown_send, error);
			}

			tcp::socket m_socket;
			beast::flat_buffer m_buffer;
			std::optional<http::request_parser<http::string_body>> m_parser;
			Request m_request;
			Handler m_handler;
		};

		class Listener final
			: public std::enable_shared_from_this<Listener>
		{
		public:
			Listener(
				asio::io_context& context, const tcp::endpoint& endpoint,
				HttpSession::Handler handler)
				: m_context(context)
				, m_acceptor(asio::make_strand(context))
				, m_handler(std::move(handler))
			{
				beast::error_code error;
				m_acceptor.open(endpoint.protocol(), error);
				if (error)
					throw beast::system_error(error);
				m_acceptor.set_option(
					asio::socket_base::reuse_address(true), error);
				if (error)
					throw beast::system_error(error);
				m_acceptor.bind(endpoint, error);
				if (error)
					throw beast::system_error(error);
				m_acceptor.listen(
					asio::socket_base::max_listen_connections, error);
				if (error)
					throw beast::system_error(error);
			}

			void Run()
			{
				Accept();
			}

			void Close() noexcept
			{
				beast::error_code error;
				m_acceptor.cancel(error);
				m_acceptor.close(error);
			}

		private:
			void Accept()
			{
				m_acceptor.async_accept(
					asio::make_strand(m_context),
					beast::bind_front_handler(
						&Listener::OnAccept, shared_from_this()));
			}

			void OnAccept(
				const beast::error_code& error, tcp::socket socket)
			{
				if (!error)
					std::make_shared<HttpSession>(
						std::move(socket), m_handler)->Run();
				if (m_acceptor.is_open())
					Accept();
			}

			asio::io_context& m_context;
			tcp::acceptor m_acceptor;
			HttpSession::Handler m_handler;
		};
	}

	class WebUIHost::Impl final
	{
	public:
		bool Start(WebUIOptions options)
		{
			std::scoped_lock stateLock(m_stateMutex);
			if (m_running.load())
				return true;

			options.workerThreads = std::clamp<std::size_t>(
				options.workerThreads, 1, 8);
			const auto assetRoot = ResolveAssetRoot(options.assetRoot);
			if (assetRoot.empty())
			{
				OutputDebugStringA(
					"WebUIHost: qBittorrent WebUI assets were not found.\n");
				return false;
			}

			try
			{
				::OpenNet::Core::AppSettingsDatabase::Instance().Initialize();
				auto& database =
					::OpenNet::Core::AppSettingsDatabase::Instance();
				options.address = database.GetString(
					"webui_host", "address").value_or(options.address);
				options.username = database.GetString(
					"webui_host", "username").value_or(options.username);
				options.password = database.GetString(
					"webui_host", "password").value_or(options.password);
				options.locale = database.GetString(
					"webui_host", "locale").value_or(options.locale);
				if (const auto port =
					database.GetInt("webui_host", "port");
					port && *port > 0
					&& *port <= std::numeric_limits<
					std::uint16_t>::max())
				{
					options.port = static_cast<std::uint16_t>(*port);
				}

				const auto address = asio::ip::make_address(options.address);
				m_context = std::make_unique<asio::io_context>(
					static_cast<int>(options.workerThreads));
				m_options = std::move(options);
				m_assetRoot = assetRoot;
				m_cacheId = RandomHex(8);
				m_languageOptions = LanguageOptions(m_assetRoot);
				LoadClientData();
				LoadTorrentMetadata();
				LoadRssRules();

				m_listener = std::make_shared<Listener>(
					*m_context,
					tcp::endpoint(address, m_options.port),
					[this](const Request& request, const tcp::endpoint& endpoint)
				{
					return HandleRequest(request, endpoint);
				});
				m_listener->Run();

				m_threads.reserve(m_options.workerThreads);
				for (std::size_t index = 0;
					 index < m_options.workerThreads; ++index)
				{
					m_threads.emplace_back([this]
					{
						m_context->run();
					});
				}
				m_running.store(true);
				OutputDebugStringA((
					"WebUIHost: Listening on http://" + m_options.address
					+ ":" + std::to_string(m_options.port) + "/\n").c_str());
				return true;
			}
			catch (const std::exception& exception)
			{
				OutputDebugStringA((
					"WebUIHost: Start failed: "
					+ std::string(exception.what()) + "\n").c_str());
				StopUnlocked();
				return false;
			}
		}

		void Stop() noexcept
		{
			std::scoped_lock stateLock(m_stateMutex);
			StopUnlocked();
		}

		bool Restart()
		{
			WebUIOptions options;
			{
				std::scoped_lock stateLock(m_stateMutex);
				options = m_options;
				StopUnlocked();
			}
			return Start(std::move(options));
		}

		[[nodiscard]] bool IsRunning() const noexcept
		{
			return m_running.load();
		}

		[[nodiscard]] std::uint16_t Port() const noexcept
		{
			std::scoped_lock stateLock(m_stateMutex);
			return m_options.port;
		}

		[[nodiscard]] std::filesystem::path AssetRoot() const
		{
			std::scoped_lock stateLock(m_stateMutex);
			return m_assetRoot;
		}

	private:
		struct SessionState
		{
			std::chrono::steady_clock::time_point touched;
			std::uint32_t responseId{};
		};

		struct FailedLogin
		{
			int attempts{};
			std::chrono::steady_clock::time_point firstAttempt;
			std::chrono::steady_clock::time_point bannedUntil;
		};

		struct SearchJob
		{
			int id{};
			bool running{};
			Json results = Json::array();
		};

		struct CreatorJob
		{
			Json status = Json::object();
			std::string torrentData;
		};

		void StopUnlocked() noexcept
		{
			if (m_listener)
				m_listener->Close();
			if (m_context)
				m_context->stop();
			for (auto& thread : m_threads)
			{
				if (thread.joinable())
					thread.join();
			}
			m_threads.clear();
			m_listener.reset();
			m_context.reset();
			{
				std::scoped_lock lock(m_sessionMutex);
				m_sessions.clear();
			}
			{
				std::scoped_lock lock(m_loginMutex);
				m_failedLogins.clear();
			}
			m_running.store(false);
		}

		bool ValidateHost(const Request& request) const
		{
			const auto iterator = request.find(http::field::host);
			if (iterator == request.end())
				return false;
			std::string host = ToLower(std::string(iterator->value()));
			const std::string port = ":" + std::to_string(m_options.port);
			if (host.ends_with(port))
				host.resize(host.size() - port.size());
			return host == "localhost"
				|| host == "127.0.0.1"
				|| host == "[::1]"
				|| host == ToLower(m_options.address);
		}

		bool ValidateOrigin(const Request& request) const
		{
			const auto originIterator = request.find(http::field::origin);
			if (originIterator == request.end() || originIterator->value().empty())
				return true;
			const auto hostIterator = request.find(http::field::host);
			if (hostIterator == request.end())
				return false;

			std::string origin = ToLower(std::string(originIterator->value()));
			const auto scheme = origin.find("://");
			if (scheme == std::string::npos)
				return false;
			origin.erase(0, scheme + 3);
			const auto slash = origin.find('/');
			if (slash != std::string::npos)
				origin.resize(slash);
			return origin == ToLower(std::string(hostIterator->value()));
		}

		std::optional<std::string> Authenticate(const Request& request)
		{
			if (const auto bearer = BearerToken(request))
			{
				std::scoped_lock lock(m_authMutex);
				return !m_apiKey.empty()
					&& ConstantTimeEquals(*bearer, m_apiKey)
					? std::optional<std::string>{
					std::string(ApiKeySession)}
				: std::nullopt;
			}
			const auto iterator = request.find(http::field::cookie);
			if (iterator == request.end())
				return std::nullopt;
			const auto cookies = ParseCookie(iterator->value());
			const auto cookie = cookies.find(CookieName());
			if (cookie == cookies.end())
				return std::nullopt;

			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock lock(m_sessionMutex);
			CleanupSessions(now);
			const auto session = m_sessions.find(cookie->second);
			if (session == m_sessions.end())
				return std::nullopt;
			session->second.touched = now;
			return cookie->second;
		}

		std::optional<std::string> BearerToken(
			const Request& request) const
		{
			const auto iterator =
				request.find(http::field::authorization);
			if (iterator == request.end())
				return std::nullopt;
			std::string_view value(
				iterator->value().data(),
				iterator->value().size());
			const auto separator = value.find(' ');
			if (separator == std::string_view::npos
				|| ToLower(std::string(value.substr(0, separator)))
				!= "bearer")
			{
				return std::nullopt;
			}
			value.remove_prefix(separator + 1);
			while (!value.empty() && value.front() == ' ')
				value.remove_prefix(1);
			return std::string(value);
		}

		bool IsValidApiKeyRequest(const Request& request)
		{
			const auto token = BearerToken(request);
			if (!token)
				return false;
			std::scoped_lock lock(m_authMutex);
			return !m_apiKey.empty()
				&& ConstantTimeEquals(*token, m_apiKey);
		}

		std::string CookieName() const
		{
			return "QBT_SID_" + std::to_string(m_options.port);
		}

		void CleanupSessions(
			const std::chrono::steady_clock::time_point now)
		{
			std::erase_if(m_sessions, [now](const auto& item)
			{
				return (now - item.second.touched) > SessionLifetime;
			});
		}

		std::string CreateSession()
		{
			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock lock(m_sessionMutex);
			CleanupSessions(now);
			std::string sessionId;
			do
			{
				sessionId = RandomHex(32);
			}
			while (m_sessions.contains(sessionId));
			m_sessions.emplace(sessionId, SessionState{ now, 0 });
			return sessionId;
		}

		std::uint32_t NextResponseId(const std::string& sessionId)
		{
			std::scoped_lock lock(m_sessionMutex);
			auto& responseId = m_sessions[sessionId].responseId;
			if (responseId == std::numeric_limits<std::uint32_t>::max())
				responseId = 1;
			else
				++responseId;
			return responseId;
		}

		bool IsLoginBanned(const std::string& client)
		{
			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock lock(m_loginMutex);
			const auto item = m_failedLogins.find(client);
			if (item == m_failedLogins.end())
				return false;
			if (item->second.bannedUntil > now)
				return true;
			if ((now - item->second.firstAttempt)
				> std::chrono::minutes(10))
			{
				m_failedLogins.erase(item);
			}
			return false;
		}

		void RegisterLoginFailure(const std::string& client)
		{
			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock lock(m_loginMutex);
			auto& failure = m_failedLogins[client];
			if (failure.attempts == 0
				|| (now - failure.firstAttempt)
					> std::chrono::minutes(10))
			{
				failure = FailedLogin{
					0, now,
					std::chrono::steady_clock::time_point{}
				};
			}
			++failure.attempts;
			if (failure.attempts >= 5)
				failure.bannedUntil = now + std::chrono::hours(1);
		}

		void ClearLoginFailures(const std::string& client)
		{
			std::scoped_lock lock(m_loginMutex);
			m_failedLogins.erase(client);
		}

		Response HandleRequest(
			const Request& request, const tcp::endpoint& endpoint)
		{
			if (!ValidateHost(request))
				return TextResponse(
					request, http::status::unauthorized, "Unauthorized");
			if ((request.method() != http::verb::get)
				&& (request.method() != http::verb::post)
				&& (request.method() != http::verb::head))
			{
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			}
			const bool bearerRequest =
				BearerToken(request).has_value();
			const bool usingApiKey = IsValidApiKeyRequest(request);
			if (bearerRequest && !usingApiKey)
				return TextResponse(
					request, http::status::unauthorized,
					"Unauthorized");
			if (request.method() == http::verb::post
				&& !usingApiKey
				&& !ValidateOrigin(request))
				return TextResponse(
					request, http::status::unauthorized, "Unauthorized");

			const auto [rawPath, query] = SplitTarget(request.target());
			const auto decodedPath = PercentDecode(rawPath, false);
			if (!decodedPath)
				return TextResponse(
					request, http::status::bad_request, "Bad Request");
			const auto sessionId = Authenticate(request);

			if (decodedPath->starts_with("/api/v2/"))
			{
				return HandleApi(
					request, *decodedPath, query, sessionId, endpoint);
			}
			if (usingApiKey)
				return TextResponse(
					request, http::status::not_found, "Not Found");
			return HandleStatic(request, *decodedPath, sessionId.has_value());
		}

		Response HandleApi(
			const Request& request, const std::string& path,
			const std::string& query,
			const std::optional<std::string>& sessionId,
			const tcp::endpoint& endpoint)
		{
			const auto queryParams = ParseKeyValue(query, false);
			auto formParams = ParseKeyValue(request.body());
			const std::string contentType =
				std::string(request[http::field::content_type]);
			const auto multipart = ToLower(contentType).starts_with(
				"multipart/form-data")
				? ParseMultipart(request.body(), contentType)
				: std::vector<FormPart>{};
			for (const auto& part : multipart)
			{
				if (part.filename.empty())
					formParams[part.name] = part.data;
			}
			auto parameters = queryParams;
			if (request.method() == http::verb::post)
			{
				for (auto& [key, value] : formParams)
					parameters[key] = std::move(value);
			}

			if (path == "/api/v2/auth/login")
			{
				if (sessionId && *sessionId == ApiKeySession)
					return TextResponse(
						request, http::status::forbidden, "Forbidden");
				if (request.method() != http::verb::post)
					return TextResponse(
						request, http::status::method_not_allowed,
						"Method Not Allowed");
				if (sessionId)
					return EmptyResponse(request);
				const std::string client =
					endpoint.address().to_string();
				if (IsLoginBanned(client))
					return TextResponse(
						request, http::status::forbidden,
						"Forbidden");

				const auto username = parameters.find("username");
				const auto password = parameters.find("password");
				bool credentialsValid = false;
				if (username != parameters.end()
					&& password != parameters.end())
				{
					std::scoped_lock lock(m_authMutex);
					credentialsValid = ConstantTimeEquals(
						username->second, m_options.username)
						&& ConstantTimeEquals(
							password->second, m_options.password);
				}
				if (!credentialsValid)
				{
					RegisterLoginFailure(client);
					return TextResponse(
						request, http::status::unauthorized, "Unauthorized");
				}

				ClearLoginFailures(client);
				const std::string createdSession = CreateSession();
				auto response = EmptyResponse(request);
				response.set(
					http::field::set_cookie,
					CookieName() + "=" + createdSession
					+ "; Path=/; HttpOnly; SameSite=Lax");
				return response;
			}

			if (!sessionId)
				return TextResponse(
					request, http::status::forbidden, "Forbidden");

			if (path == "/api/v2/auth/logout")
			{
				if (*sessionId == ApiKeySession)
					return TextResponse(
						request, http::status::forbidden, "Forbidden");
				if (request.method() != http::verb::post)
					return TextResponse(
						request, http::status::method_not_allowed,
						"Method Not Allowed");
				{
					std::scoped_lock lock(m_sessionMutex);
					m_sessions.erase(*sessionId);
				}
				auto response = TextResponse(request, http::status::ok, "");
				response.set(
					http::field::set_cookie,
					CookieName() + "=; Path=/; HttpOnly; SameSite=Lax; "
					"Max-Age=0");
				return response;
			}

			if (path == "/api/v2/app/webapiVersion")
				return TextResponse(request, http::status::ok, "2.16.0");
			if (path == "/api/v2/app/version")
				return TextResponse(request, http::status::ok, "OpenNet");
			if (path == "/api/v2/app/buildInfo")
			{
				return JsonResponse(request, Json{
					{"qt", "C++/WinRT"},
					{"libtorrent", "2.0.11"},
					{"boost", BOOST_LIB_VERSION},
					{"openssl", "OpenSSL"},
					{"zlib", "zlib"},
					{"bitness", static_cast<int>(sizeof(void*) * 8)},
					{"platform", "windows"}
									});
			}
			if (path == "/api/v2/app/processInfo")
			{
				const auto launch = std::chrono::duration_cast<std::chrono::seconds>(
					m_launchTime.time_since_epoch()).count();
				return JsonResponse(request, Json{ {"launch_time", launch} });
			}
			if (path == "/api/v2/app/defaultSavePath")
				return TextResponse(
					request, http::status::ok, DefaultSavePath());
			if (path == "/api/v2/app/preferences")
				return JsonResponse(request, Preferences());
			if (path == "/api/v2/app/setPreferences")
				return SetPreferences(request, parameters);
			if (path == "/api/v2/app/getFreeSpaceAtPath")
				return HandleGetFreeSpace(request, parameters);
			if (path == "/api/v2/app/getDirectoryContent")
				return GetDirectoryContent(request, parameters);
			if (path == "/api/v2/app/cookies")
				return JsonResponse(request, Cookies());
			if (path == "/api/v2/app/setCookies")
				return SetCookies(request, parameters);
			if (path == "/api/v2/app/rotateAPIKey")
				return RotateApiKey(request);
			if (path == "/api/v2/app/deleteAPIKey")
				return DeleteApiKey(request);
			if (path == "/api/v2/app/networkInterfaceList")
			{
				return JsonResponse(request, Json::array({
					Json{{"name", "Default"}, {"value", ""}}
														 }));
			}
			if (path == "/api/v2/app/networkInterfaceAddressList")
				return NetworkInterfaceAddresses(
					request, parameters);
			if (path == "/api/v2/app/sendTestEmail")
				return TextResponse(
					request, http::status::conflict,
					"SMTP delivery is not configured");
			if (path == "/api/v2/app/shutdown")
			{
				if (request.method() != http::verb::post)
					return TextResponse(
						request, http::status::method_not_allowed,
						"Method Not Allowed");
				if (!m_options.shutdownCallback)
					return TextResponse(
						request, http::status::conflict,
						"Application shutdown is unavailable");
				m_options.shutdownCallback();
				return EmptyResponse(request);
			}
			if (path == "/api/v2/clientdata/load")
				return LoadClientData(request, parameters);
			if (path == "/api/v2/clientdata/store")
				return StoreClientData(request, parameters);
			if (path == "/api/v2/sync/maindata")
				return MainData(request, *sessionId);
			if (path == "/api/v2/sync/torrentPeers")
				return TorrentPeers(request, parameters, *sessionId);
			if (path == "/api/v2/torrents/count")
			{
				const auto* core =
					::OpenNet::Core::P2PManager::Instance().TorrentCore();
				return TextResponse(
					request, http::status::ok,
					std::to_string(core ? core->GetTorrentCount() : 0));
			}
			if (path == "/api/v2/torrents/info")
				return JsonResponse(request, TorrentList());
			if (path == "/api/v2/torrents/properties")
				return TorrentProperties(request, parameters);
			if (path == "/api/v2/torrents/trackers")
				return TorrentTrackers(request, parameters);
			if (path == "/api/v2/torrents/webseeds")
				return TorrentPieceData(
					request, parameters, "webseeds");
			if (path == "/api/v2/torrents/files")
				return TorrentFiles(request, parameters);
			if (path == "/api/v2/torrents/pieceStates"
				|| path == "/api/v2/torrents/pieceAvailability"
				|| path == "/api/v2/torrents/pieceHashes")
			{
				return TorrentPieceData(
					request, parameters,
					path.substr(path.rfind('/') + 1));
			}
			if (path == "/api/v2/torrents/add")
				return AddTorrent(request, parameters, multipart);
			if (path == "/api/v2/torrents/start")
				return TorrentAction(request, parameters, "start");
			if (path == "/api/v2/torrents/stop")
				return TorrentAction(request, parameters, "stop");
			if (path == "/api/v2/torrents/delete")
				return TorrentAction(request, parameters, "delete");
			if (path == "/api/v2/torrents/recheck")
				return TorrentAction(request, parameters, "recheck");
			if (path == "/api/v2/torrents/reannounce")
				return TorrentAction(request, parameters, "reannounce");
			if (path == "/api/v2/torrents/filePrio")
				return SetFilePriority(request, parameters);
			if (path == "/api/v2/torrents/downloadFile"
				|| path == "/api/v2/torrents/export"
				|| path == "/api/v2/torrents/fetchMetadata"
				|| path == "/api/v2/torrents/parseMetadata"
				|| path == "/api/v2/torrents/saveMetadata"
				|| path == "/api/v2/torrents/SSLParameters"
				|| path == "/api/v2/torrents/setSSLParameters")
			{
				return TorrentExtended(
					request, parameters, multipart,
					path.substr(path.rfind('/') + 1));
			}
			if (path == "/api/v2/torrents/downloadLimit")
				return TorrentLimits(request, parameters, true);
			if (path == "/api/v2/torrents/uploadLimit")
				return TorrentLimits(request, parameters, false);
			if (path == "/api/v2/torrents/setDownloadLimit")
				return SetTorrentLimit(request, parameters, true);
			if (path == "/api/v2/torrents/setUploadLimit")
				return SetTorrentLimit(request, parameters, false);
			if (path == "/api/v2/torrents/addTrackers"
				|| path == "/api/v2/torrents/editTracker"
				|| path == "/api/v2/torrents/removeTrackers"
				|| path == "/api/v2/torrents/addWebSeeds"
				|| path == "/api/v2/torrents/editWebSeed"
				|| path == "/api/v2/torrents/removeWebSeeds"
				|| path == "/api/v2/torrents/addPeers")
			{
				return MutateTorrentConnections(
					request, parameters,
					path.substr(path.rfind('/') + 1));
			}
			if (path == "/api/v2/torrents/categories")
				return JsonResponse(request, Categories());
			if (path == "/api/v2/torrents/createCategory")
				return CreateOrEditCategory(request, parameters, false);
			if (path == "/api/v2/torrents/editCategory")
				return CreateOrEditCategory(request, parameters, true);
			if (path == "/api/v2/torrents/removeCategories")
				return RemoveCategories(request, parameters);
			if (path == "/api/v2/torrents/setCategory")
				return SetCategory(request, parameters);
			if (path == "/api/v2/torrents/tags")
				return JsonResponse(request, Tags());
			if (path == "/api/v2/torrents/createTags")
				return MutateGlobalTags(request, parameters, false);
			if (path == "/api/v2/torrents/deleteTags")
				return MutateGlobalTags(request, parameters, true);
			if (path == "/api/v2/torrents/addTags")
				return MutateTorrentTags(request, parameters, "add");
			if (path == "/api/v2/torrents/setTags")
				return MutateTorrentTags(request, parameters, "set");
			if (path == "/api/v2/torrents/removeTags")
				return MutateTorrentTags(request, parameters, "remove");
			if (path == "/api/v2/torrents/setShareLimits"
				|| path == "/api/v2/torrents/increasePrio"
				|| path == "/api/v2/torrents/decreasePrio"
				|| path == "/api/v2/torrents/topPrio"
				|| path == "/api/v2/torrents/bottomPrio"
				|| path == "/api/v2/torrents/setAutoManagement"
				|| path == "/api/v2/torrents/setForceStart"
				|| path == "/api/v2/torrents/setSuperSeeding"
				|| path == "/api/v2/torrents/toggleSequentialDownload"
				|| path == "/api/v2/torrents/toggleFirstLastPiecePrio"
				|| path == "/api/v2/torrents/setLocation"
				|| path == "/api/v2/torrents/setSavePath"
				|| path == "/api/v2/torrents/setDownloadPath"
				|| path == "/api/v2/torrents/rename"
				|| path == "/api/v2/torrents/setComment"
				|| path == "/api/v2/torrents/renameFile"
				|| path == "/api/v2/torrents/renameFolder")
			{
				return MutateTorrentState(
					request, parameters,
					path.substr(path.rfind('/') + 1));
			}
			if (path == "/api/v2/transfer/info")
				return TransferInfo(request);
			if (path == "/api/v2/transfer/downloadLimit")
			{
				const auto settings =
					::OpenNet::Core::TorrentSettingsManager::Instance().Get();
				return TextResponse(
					request, http::status::ok,
					std::to_string(m_speedLimitsMode.load()
								   ? m_altDownloadLimit.load()
								   : settings.downloadRateLimit));
			}
			if (path == "/api/v2/transfer/uploadLimit")
			{
				const auto settings =
					::OpenNet::Core::TorrentSettingsManager::Instance().Get();
				return TextResponse(
					request, http::status::ok,
					std::to_string(m_speedLimitsMode.load()
								   ? m_altUploadLimit.load()
								   : settings.uploadRateLimit));
			}
			if (path == "/api/v2/transfer/setDownloadLimit")
				return SetTransferLimit(request, parameters, true);
			if (path == "/api/v2/transfer/setUploadLimit")
				return SetTransferLimit(request, parameters, false);
			if (path == "/api/v2/transfer/getSpeedLimits")
				return GetSpeedLimits(request);
			if (path == "/api/v2/transfer/setSpeedLimits")
				return SetSpeedLimits(request, parameters);
			if (path == "/api/v2/transfer/speedLimitsMode")
				return TextResponse(
					request, http::status::ok,
					m_speedLimitsMode.load() ? "1" : "0");
			if (path == "/api/v2/transfer/setSpeedLimitsMode")
				return SetSpeedLimitsMode(request, parameters, false);
			if (path == "/api/v2/transfer/toggleSpeedLimitsMode")
				return SetSpeedLimitsMode(request, parameters, true);
			if (path == "/api/v2/transfer/banPeers")
				return BanPeers(request, parameters);
			if (path == "/api/v2/log/main"
				|| path == "/api/v2/log/peers")
			{
				return JsonResponse(request, Json::array());
			}
			if (path == "/api/v2/rss/addFeed"
				|| path == "/api/v2/rss/addFolder"
				|| path == "/api/v2/rss/items"
				|| path == "/api/v2/rss/markAsRead"
				|| path == "/api/v2/rss/moveItem"
				|| path == "/api/v2/rss/refreshItem"
				|| path == "/api/v2/rss/removeItem"
				|| path == "/api/v2/rss/setFeedURL"
				|| path == "/api/v2/rss/setFeedRefreshInterval"
				|| path == "/api/v2/rss/cloneRule"
				|| path == "/api/v2/rss/matchingArticles"
				|| path == "/api/v2/rss/removeRule"
				|| path == "/api/v2/rss/renameRule"
				|| path == "/api/v2/rss/rules"
				|| path == "/api/v2/rss/setRule")
			{
				return RssApi(
					request, parameters,
					path.substr(path.rfind('/') + 1));
			}
			if (path == "/api/v2/search/start"
				|| path == "/api/v2/search/stop"
				|| path == "/api/v2/search/status"
				|| path == "/api/v2/search/results"
				|| path == "/api/v2/search/delete"
				|| path == "/api/v2/search/downloadTorrent"
				|| path == "/api/v2/search/plugins"
				|| path == "/api/v2/search/installPlugin"
				|| path == "/api/v2/search/uninstallPlugin"
				|| path == "/api/v2/search/enablePlugin"
				|| path == "/api/v2/search/updatePlugins")
			{
				return SearchApi(
					request, parameters,
					path.substr(path.rfind('/') + 1));
			}
			if (path == "/api/v2/torrentcreator/addTask"
				|| path == "/api/v2/torrentcreator/status"
				|| path == "/api/v2/torrentcreator/torrentFile"
				|| path == "/api/v2/torrentcreator/deleteTask")
			{
				return TorrentCreatorApi(
					request, parameters,
					path.substr(path.rfind('/') + 1));
			}

			return TextResponse(
				request, http::status::not_found, "Not Found");
		}

		Response HandleStatic(
			const Request& request, const std::string& decodedPath,
			const bool authenticated)
		{
			if (decodedPath.empty() || decodedPath.front() != '/'
				|| decodedPath.find('\\') != std::string::npos
				|| decodedPath.find(':') != std::string::npos)
			{
				return TextResponse(
					request, http::status::bad_request, "Bad Request");
			}

			std::filesystem::path relative;
			const std::string normalizedPath =
				(decodedPath == "/") ? "/index.html" : decodedPath;
			std::size_t cursor = 1;
			while (cursor <= normalizedPath.size())
			{
				const auto separator = normalizedPath.find('/', cursor);
				const auto segment = normalizedPath.substr(
					cursor, separator - cursor);
				if (segment == "." || segment == ".." || segment.empty())
					return TextResponse(
						request, http::status::bad_request, "Bad Request");
				relative /= PathFromUtf8(segment);
				if (separator == std::string::npos)
					break;
				cursor = separator + 1;
			}

			const auto selectedRoot =
				m_assetRoot / (authenticated ? L"private" : L"public");
			auto file = selectedRoot / relative;
			std::error_code error;
			if (!std::filesystem::is_regular_file(file, error)
				&& authenticated)
			{
				file = m_assetRoot / L"public" / relative;
			}
			if (!std::filesystem::is_regular_file(file, error)
				|| !IsPathInside(m_assetRoot, file))
			{
				return TextResponse(
					request, http::status::not_found, "Not Found");
			}

			const auto size = std::filesystem::file_size(file, error);
			if (error || size > MaxStaticFile)
				return TextResponse(
					request, http::status::internal_server_error,
					"Internal Server Error");

			std::ifstream stream(file, std::ios::binary);
			if (!stream)
				return TextResponse(
					request, http::status::not_found, "Not Found");
			std::string body{
				std::istreambuf_iterator<char>{stream},
				std::istreambuf_iterator<char>{} };

			if (IsTextAsset(file))
			{
				body = ReplaceAll(
					std::move(body), "${LANG}", m_options.locale);
				body = ReplaceAll(
					std::move(body), "${CACHEID}", m_cacheId);
				body = ReplaceAll(
					std::move(body), "${LANGUAGE_OPTIONS}", m_languageOptions);
				body = StripTranslationMarkers(std::move(body));
			}

			auto response = TextResponse(
				request, http::status::ok, std::move(body), MimeType(file));
			const std::string mime = MimeType(file);
			if (mime.starts_with("image/"))
			{
				response.set(
					http::field::cache_control,
					"private, max-age=604800");
			}
			else if (mime.starts_with("text/css")
					 || mime.starts_with("text/javascript"))
			{
				response.set(
					http::field::cache_control,
					"private, max-age=43200");
			}
			else
			{
				response.set(http::field::cache_control, "no-store");
			}
			return response;
		}

		Json Preferences()
		{
			const auto settings =
				::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			std::string apiKey;
			std::string username;
			{
				std::scoped_lock lock(m_authMutex);
				apiKey = m_apiKey;
				username = m_options.username;
			}
			Json value{
				{"app_instance_name", "OpenNet"},
				{"locale", m_options.locale},
				{"web_ui_session_timeout", 3600},
				{"web_ui_max_auth_fail_count", 5},
				{"web_ui_ban_duration", 3600},
				{"web_ui_port", m_options.port},
				{"web_ui_address", m_options.address},
				{"web_ui_upnp", false},
				{"web_ui_csrf_protection_enabled", true},
				{"web_ui_host_header_validation_enabled", true},
				{"web_ui_clickjacking_protection_enabled", true},
				{"web_ui_secure_cookie_enabled", false},
				{"web_ui_https_enabled", false},
				{"web_ui_reverse_proxy_enabled", false},
				{"web_ui_use_custom_http_headers_enabled", false},
				{"web_ui_username", std::move(username)},
				{"web_ui_api_key", std::move(apiKey)},
				{"save_path", DefaultSavePath()},
				{"temp_path_enabled", false},
				{"temp_path", ""},
				{"autorun_enabled", false},
				{"autorun_program", ""},
				{"confirm_torrent_deletion", true},
				{"confirm_torrent_recheck", true},
				{"delete_torrent_content_files", false},
				{"dht", settings.enableDht},
				{"pex", true},
				{"lsd", settings.enableLsd},
				{"upnp", settings.enableUpnp},
				{"queueing_enabled", true},
				{"max_active_downloads", settings.activeDownloads},
				{"max_active_uploads", settings.activeSeeds},
				{"max_active_torrents", settings.activeLimit},
				{"dl_limit", settings.downloadRateLimit},
				{"up_limit", settings.uploadRateLimit},
				{"status_bar_external_ip", true},
				{"alternative_webui_enabled", false},
				{"rss_processing_enabled", false},
				{"mail_notification_enabled", false},
				{"scheduler_enabled", false}
			};
			std::scoped_lock lock(m_preferenceMutex);
			value.update(m_preferenceOverrides);
			return value;
		}

		Response SetPreferences(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			const auto data = parameters.find("json");
			if (data == parameters.end())
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: json");
			try
			{
				const Json parsed = Json::parse(data->second);
				if (!parsed.is_object())
					throw std::invalid_argument("json must be an object");

				auto settings =
					::OpenNet::Core::TorrentSettingsManager::Instance().Get();
				const auto setInteger = [&parsed](
					const char* key, int& destination)
				{
					if (!parsed.contains(key))
						return;
					const auto& value = parsed.at(key);
					if (!value.is_number_integer())
						throw std::invalid_argument(
							std::string(key) + " must be an integer");
					const auto integer = value.get<std::int64_t>();
					if (integer < 0
						|| integer > std::numeric_limits<int>::max())
					{
						throw std::out_of_range(
							std::string(key) + " is out of range");
					}
					destination = static_cast<int>(integer);
				};
				const auto setBoolean = [&parsed](
					const char* key, bool& destination)
				{
					if (!parsed.contains(key))
						return;
					const auto& value = parsed.at(key);
					if (!value.is_boolean())
						throw std::invalid_argument(
							std::string(key) + " must be a boolean");
					destination = value.get<bool>();
				};

				setInteger("dl_limit", settings.downloadRateLimit);
				setInteger("up_limit", settings.uploadRateLimit);
				setInteger("max_active_downloads", settings.activeDownloads);
				setInteger("max_active_uploads", settings.activeSeeds);
				setInteger("max_active_torrents", settings.activeLimit);
				setInteger("max_connec", settings.connectionsLimit);
				setBoolean("dht", settings.enableDht);
				setBoolean("lsd", settings.enableLsd);
				setBoolean("upnp", settings.enableUpnp);
				setBoolean("anonymous_mode", settings.anonymousMode);
				if (parsed.contains("save_path"))
				{
					if (!parsed.at("save_path").is_string())
						throw std::invalid_argument(
							"save_path must be a string");
					settings.defaultSavePath = PathFromUtf8(
						parsed.at("save_path").get<std::string>()).wstring();
				}
				std::optional<std::string> username;
				std::optional<std::string> password;
				if (parsed.contains("web_ui_username"))
				{
					if (!parsed.at("web_ui_username").is_string())
						throw std::invalid_argument(
							"web_ui_username must be a string");
					username = parsed.at(
						"web_ui_username").get<std::string>();
					if (username->size() < 3
						|| username->find(':') != std::string::npos)
					{
						throw std::invalid_argument(
							"Invalid WebUI username");
					}
				}
				if (parsed.contains("web_ui_password"))
				{
					if (!parsed.at("web_ui_password").is_string())
						throw std::invalid_argument(
							"web_ui_password must be a string");
					password = parsed.at(
						"web_ui_password").get<std::string>();
					if (password->size() < 6)
						throw std::invalid_argument(
							"WebUI password must be at least 6 characters");
				}
				if (parsed.contains("locale"))
				{
					if (!parsed.at("locale").is_string())
						throw std::invalid_argument(
							"locale must be a string");
					::OpenNet::Core::AppSettingsDatabase::Instance()
						.SetString(
							"webui_host", "locale",
							parsed.at("locale").get<std::string>());
				}
				if (parsed.contains("web_ui_address"))
				{
					if (!parsed.at("web_ui_address").is_string())
						throw std::invalid_argument(
							"web_ui_address must be a string");
					const auto address = parsed.at(
						"web_ui_address").get<std::string>();
					asio::ip::make_address(address);
					::OpenNet::Core::AppSettingsDatabase::Instance()
						.SetString(
							"webui_host", "address", address);
				}
				if (parsed.contains("web_ui_port"))
				{
					const auto& port = parsed.at("web_ui_port");
					if (!port.is_number_integer())
						throw std::invalid_argument(
							"web_ui_port must be an integer");
					const auto value = port.get<std::int64_t>();
					if (value <= 0
						|| value > std::numeric_limits<
						std::uint16_t>::max())
					{
						throw std::out_of_range(
							"web_ui_port is out of range");
					}
					::OpenNet::Core::AppSettingsDatabase::Instance()
						.SetInt("webui_host", "port", value);
				}
				PersistAndApplyTorrentSettings(settings);
				if (username || password)
				{
					std::scoped_lock authLock(m_authMutex);
					if (username)
					{
						m_options.username = *username;
						::OpenNet::Core::AppSettingsDatabase::Instance()
							.SetString(
								"webui_host", "username", *username);
					}
					if (password)
					{
						m_options.password = *password;
						::OpenNet::Core::AppSettingsDatabase::Instance()
							.SetString(
								"webui_host", "password", *password);
						::OpenNet::Core::AppSettingsDatabase::Instance()
							.SetBool(
								"webui_host", "initialized", true);
					}
				}
				{
					std::scoped_lock lock(m_preferenceMutex);
					for (const auto& [key, value] : parsed.items())
					{
						if (key == "web_ui_password")
							continue;
						m_preferenceOverrides[key] = value;
						::OpenNet::Core::AppSettingsDatabase::Instance().SetString(
							"webui_preferences", key, value.dump());
					}
				}
				return EmptyResponse(request);
			}
			catch (const std::exception& exception)
			{
				return TextResponse(
					request, http::status::bad_request, exception.what());
			}
		}

		Response HandleGetFreeSpace(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			const auto item = parameters.find("path");
			if (item == parameters.end())
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: path");
			std::filesystem::path path = PathFromUtf8(item->second);
			std::error_code error;
			while (!path.empty() && !std::filesystem::exists(path, error))
				path = path.parent_path();
			if (path.empty())
				return TextResponse(request, http::status::ok, "-1");
			const auto space = std::filesystem::space(path, error);
			return TextResponse(
				request, http::status::ok,
				error ? "-1" : std::to_string(space.available));
		}

		Response GetDirectoryContent(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			const auto item = parameters.find("dirPath");
			if (item == parameters.end())
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: dirPath");
			const auto directory = PathFromUtf8(item->second);
			if (!directory.is_absolute())
				return TextResponse(
					request, http::status::bad_request,
					"Invalid directory path");
			std::error_code error;
			if (!std::filesystem::is_directory(directory, error))
				return TextResponse(
					request, http::status::not_found,
					"Directory does not exist");

			const std::string mode = parameters.contains("mode")
				? parameters.at("mode") : "all";
			const bool metadata = parameters.contains("withMetadata")
				&& (parameters.at("withMetadata") == "true"
					|| parameters.at("withMetadata") == "1");
			Json result = Json::array();
			for (const auto& entry : std::filesystem::directory_iterator(
				directory,
				std::filesystem::directory_options::skip_permission_denied,
				error))
			{
				const bool isDirectory = entry.is_directory(error);
				const bool isFile = entry.is_regular_file(error);
				if ((mode == "dirs" && !isDirectory)
					|| (mode == "files" && !isFile)
					|| (!isDirectory && !isFile))
				{
					continue;
				}
				if (!metadata)
				{
					result.push_back(PathUtf8(entry.path()));
					continue;
				}
				Json file{
					{"name", PathUtf8(entry.path().filename())},
					{"type", isDirectory ? "dir" : "file"},
					{"creation_date", -1},
					{"last_access_date", -1},
					{"last_modification_date", -1}
				};
				if (isFile)
					file["size"] = entry.file_size(error);
				result.push_back(std::move(file));
			}
			return JsonResponse(request, result);
		}

		Json Cookies()
		{
			std::scoped_lock lock(m_cookieMutex);
			return m_cookies;
		}

		Response SetCookies(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("cookies"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: cookies");
			try
			{
				const Json cookies =
					Json::parse(parameters.at("cookies"));
				if (!cookies.is_array())
					throw std::invalid_argument(
						"cookies must be array");
				for (const auto& cookie : cookies)
				{
					if (!cookie.is_object())
						throw std::invalid_argument(
							"cookies must contain objects");
				}
				{
					std::scoped_lock lock(m_cookieMutex);
					m_cookies = cookies;
				}
				::OpenNet::Core::AppSettingsDatabase::Instance().SetString(
					"webui_http", "cookies", cookies.dump());
				return EmptyResponse(request);
			}
			catch (const std::exception& exception)
			{
				return TextResponse(
					request, http::status::bad_request,
					exception.what());
			}
		}

		Response RotateApiKey(const Request& request)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			const std::string key = "qbt_" + RandomHex(14);
			{
				std::scoped_lock lock(m_authMutex);
				m_apiKey = key;
			}
			::OpenNet::Core::AppSettingsDatabase::Instance().SetString(
				"webui_http", "api_key", key);
			return JsonResponse(request, Json{ {"apiKey", key} });
		}

		Response DeleteApiKey(const Request& request)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			{
				std::scoped_lock lock(m_authMutex);
				m_apiKey.clear();
			}
			::OpenNet::Core::AppSettingsDatabase::Instance().Delete(
				"webui_http", "api_key");
			return EmptyResponse(request);
		}

		Response NetworkInterfaceAddresses(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (!parameters.contains("iface"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: iface");
			std::unordered_set<std::string> addresses{
				"127.0.0.1", "::1"
			};
			try
			{
				asio::io_context context;
				tcp::resolver resolver(context);
				for (const auto& result :
					 resolver.resolve(asio::ip::host_name(), "0"))
				{
					addresses.insert(
						result.endpoint().address().to_string());
				}
			}
			catch (...)
			{
			}
			std::vector<std::string> sorted(
				addresses.begin(), addresses.end());
			std::ranges::sort(sorted);
			return JsonResponse(request, sorted);
		}

		void LoadClientData()
		{
			std::scoped_lock lock(m_clientDataMutex);
			m_clientData = DefaultClientData();
			for (const auto& entry :
				 ::OpenNet::Core::AppSettingsDatabase::Instance().GetCategory(
					 "webui_clientdata"))
			{
				try
				{
					m_clientData[entry.key] = Json::parse(entry.value);
				}
				catch (...)
				{
				}
			}
			for (const auto& entry :
				 ::OpenNet::Core::AppSettingsDatabase::Instance().GetCategory(
					 "webui_preferences"))
			{
				try
				{
					m_preferenceOverrides[entry.key] = Json::parse(entry.value);
				}
				catch (...)
				{
				}
			}
			{
				std::scoped_lock authLock(m_authMutex);
				m_apiKey =
					::OpenNet::Core::AppSettingsDatabase::Instance()
					.GetString("webui_http", "api_key")
					.value_or("");
			}
			if (const auto cookies =
				::OpenNet::Core::AppSettingsDatabase::Instance().GetString(
					"webui_http", "cookies"))
			{
				try
				{
					const Json parsed = Json::parse(*cookies);
					if (parsed.is_array())
					{
						std::scoped_lock cookieLock(m_cookieMutex);
						m_cookies = parsed;
					}
				}
				catch (...)
				{
				}
			}
			auto& database =
				::OpenNet::Core::AppSettingsDatabase::Instance();
			m_altDownloadLimit.store(static_cast<int>(
				std::clamp<std::int64_t>(
					database.GetInt(
						"webui_transfer", "alt_download_limit")
					.value_or(10 * 1024),
					std::int64_t{ 0 },
					static_cast<std::int64_t>(
						std::numeric_limits<int>::max()))));
			m_altUploadLimit.store(static_cast<int>(
				std::clamp<std::int64_t>(
					database.GetInt(
						"webui_transfer", "alt_upload_limit")
					.value_or(10 * 1024),
					std::int64_t{ 0 },
					static_cast<std::int64_t>(
						std::numeric_limits<int>::max()))));
			m_speedLimitsMode.store(
				database.GetBool(
					"webui_transfer", "alternative_mode")
				.value_or(false));
		}

		Response LoadClientData(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			std::scoped_lock lock(m_clientDataMutex);
			if (!parameters.contains("keys") || parameters.at("keys").empty())
				return JsonResponse(request, m_clientData);
			try
			{
				const Json keys = Json::parse(parameters.at("keys"));
				if (!keys.is_array())
					throw std::invalid_argument("keys must be an array");
				Json result = Json::object();
				for (const auto& key : keys)
				{
					if (!key.is_string())
						throw std::invalid_argument("keys must contain strings");
					const auto name = key.get<std::string>();
					if (m_clientData.contains(name))
						result[name] = m_clientData.at(name);
				}
				return JsonResponse(request, result);
			}
			catch (const std::exception& exception)
			{
				return TextResponse(
					request, http::status::bad_request, exception.what());
			}
		}

		Response StoreClientData(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			const auto data = parameters.find("data");
			if (data == parameters.end())
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: data");
			try
			{
				const Json parsed = Json::parse(data->second);
				if (!parsed.is_object())
					throw std::invalid_argument("data must be an object");
				std::scoped_lock lock(m_clientDataMutex);
				for (const auto& [key, value] : parsed.items())
				{
					if (value.is_null())
					{
						m_clientData.erase(key);
						::OpenNet::Core::AppSettingsDatabase::Instance().Delete(
							"webui_clientdata", key);
					}
					else
					{
						m_clientData[key] = value;
						::OpenNet::Core::AppSettingsDatabase::Instance().SetString(
							"webui_clientdata", key, value.dump());
					}
				}
				return EmptyResponse(request);
			}
			catch (const std::exception& exception)
			{
				return TextResponse(
					request, http::status::bad_request, exception.what());
			}
		}

		void LoadTorrentMetadata()
		{
			std::scoped_lock lock(m_metadataMutex);
			const auto load = [](const char* key, const Json& fallback)
			{
				const auto value =
					::OpenNet::Core::AppSettingsDatabase::Instance().GetString(
						"webui_torrent_metadata", key);
				if (!value)
					return fallback;
				try
				{
					return Json::parse(*value);
				}
				catch (...)
				{
					return fallback;
				}
			};

			m_categories = load("categories", Json::object());
			m_tags = load("tags", Json::array());
			m_torrentCategories =
				load("torrent_categories", Json::object());
			m_torrentTags = load("torrent_tags", Json::object());
			m_torrentOverrides =
				load("torrent_overrides", Json::object());
			if (!m_categories.is_object())
				m_categories = Json::object();
			if (!m_tags.is_array())
				m_tags = Json::array();
			if (!m_torrentCategories.is_object())
				m_torrentCategories = Json::object();
			if (!m_torrentTags.is_object())
				m_torrentTags = Json::object();
			if (!m_torrentOverrides.is_object())
				m_torrentOverrides = Json::object();
		}

		void PersistTorrentMetadataLocked()
		{
			auto& database =
				::OpenNet::Core::AppSettingsDatabase::Instance();
			database.SetBatch({
				{"categories", m_categories.dump(),
					"webui_torrent_metadata"},
				{"tags", m_tags.dump(),
					"webui_torrent_metadata"},
				{"torrent_categories", m_torrentCategories.dump(),
					"webui_torrent_metadata"},
				{"torrent_tags", m_torrentTags.dump(),
					"webui_torrent_metadata"},
				{"torrent_overrides", m_torrentOverrides.dump(),
					"webui_torrent_metadata"}
							  });
		}

		void ApplyTorrentMetadata(Json& torrent)
		{
			const std::string hash =
				torrent.at("hash").get<std::string>();
			std::scoped_lock lock(m_metadataMutex);
			if (m_torrentCategories.contains(hash)
				&& m_torrentCategories.at(hash).is_string())
			{
				torrent["category"] =
					m_torrentCategories.at(hash).get<std::string>();
			}
			if (m_torrentOverrides.contains(hash)
				&& m_torrentOverrides.at(hash).is_object())
			{
				const auto& overrides =
					m_torrentOverrides.at(hash);
				for (const auto* key : {
					"name", "comment", "download_path",
					"auto_tmm", "max_ratio",
					"max_seeding_time",
					"max_inactive_seeding_time",
					"share_limit_action",
					"share_limit_mode" })
				{
					if (overrides.contains(key))
						torrent[key] = overrides.at(key);
				}
			}

			if (!m_torrentTags.contains(hash)
				|| !m_torrentTags.at(hash).is_array())
			{
				return;
			}
			std::string tags;
			for (const auto& value : m_torrentTags.at(hash))
			{
				if (!value.is_string())
					continue;
				if (!tags.empty())
					tags += ", ";
				tags += value.get<std::string>();
			}
			torrent["tags"] = std::move(tags);
		}

		Json Categories()
		{
			std::scoped_lock lock(m_metadataMutex);
			return m_categories;
		}

		Json Tags()
		{
			std::scoped_lock lock(m_metadataMutex);
			return m_tags;
		}

		std::vector<std::string> ResolveTorrentHashes(
			const std::string& hashes)
		{
			const Json torrents = TorrentList();
			std::vector<std::string> result;
			if (hashes == "all")
			{
				for (const auto& torrent : torrents)
					result.push_back(
						torrent.at("hash").get<std::string>());
				return result;
			}

			const auto requestedValues = SplitValues(hashes, '|');
			const std::unordered_set<std::string> requested(
				requestedValues.begin(), requestedValues.end());
			for (const auto& torrent : torrents)
			{
				const auto hash =
					torrent.at("hash").get<std::string>();
				if (requested.contains(hash))
					result.push_back(hash);
			}
			return result;
		}

		Response CreateOrEditCategory(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const bool edit)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			const auto categoryItem = parameters.find("category");
			if (categoryItem == parameters.end()
				|| categoryItem->second.empty())
			{
				return TextResponse(
					request, http::status::bad_request,
					"Category cannot be empty");
			}
			const auto& name = categoryItem->second;
			if (name.find('\n') != std::string::npos
				|| name.find('\r') != std::string::npos)
			{
				return TextResponse(
					request, http::status::conflict,
					"Incorrect category name");
			}

			const std::string savePath = parameters.contains("savePath")
				? parameters.at("savePath") : "";
			Json downloadPath = nullptr;
			if (parameters.contains("downloadPathEnabled"))
			{
				const auto enabled =
					ToLower(parameters.at("downloadPathEnabled"));
				if (enabled == "true" || enabled == "1")
				{
					downloadPath = parameters.contains("downloadPath")
						? Json(parameters.at("downloadPath"))
						: Json("");
				}
				else if (enabled == "false" || enabled == "0")
				{
					downloadPath = false;
				}
			}

			std::scoped_lock lock(m_metadataMutex);
			if (edit && !m_categories.contains(name))
				return TextResponse(
					request, http::status::not_found,
					"Category does not exist");
			if (!edit && m_categories.contains(name))
				return TextResponse(
					request, http::status::conflict,
					"Unable to create category");
			m_categories[name] = Json{
				{"name", name},
				{"savePath", savePath},
				{"download_path", std::move(downloadPath)},
				{"ratio_limit", -2},
				{"seeding_time_limit", -2},
				{"inactive_seeding_time_limit", -2},
				{"share_limit_action", "default"},
				{"share_limit_mode", "default"}
			};
			PersistTorrentMetadataLocked();
			return EmptyResponse(request);
		}

		Response RemoveCategories(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("categories"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: categories");
			const auto names =
				SplitValues(parameters.at("categories"), '\n');
			const std::unordered_set<std::string> removed(
				names.begin(), names.end());
			std::scoped_lock lock(m_metadataMutex);
			for (const auto& name : names)
				m_categories.erase(name);
			for (auto& [hash, category] :
				 m_torrentCategories.items())
			{
				if (category.is_string()
					&& removed.contains(category.get<std::string>()))
				{
					category = "";
				}
			}
			PersistTorrentMetadataLocked();
			return EmptyResponse(request);
		}

		Response SetCategory(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("hashes")
				|| !parameters.contains("category"))
			{
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter");
			}
			const auto hashes =
				ResolveTorrentHashes(parameters.at("hashes"));
			const auto& category = parameters.at("category");
			std::scoped_lock lock(m_metadataMutex);
			if (!category.empty() && !m_categories.contains(category))
				return TextResponse(
					request, http::status::conflict,
					"Incorrect category name");
			for (const auto& hash : hashes)
				m_torrentCategories[hash] = category;
			PersistTorrentMetadataLocked();
			return EmptyResponse(request);
		}

		Response MutateGlobalTags(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const bool remove)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("tags"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: tags");
			const auto values = SplitValues(parameters.at("tags"), ',');
			std::scoped_lock lock(m_metadataMutex);
			std::unordered_set<std::string> tags;
			for (const auto& tag : m_tags)
			{
				if (tag.is_string())
					tags.insert(tag.get<std::string>());
			}
			for (const auto& tag : values)
			{
				if (remove)
					tags.erase(tag);
				else
					tags.insert(tag);
			}
			if (remove)
			{
				for (auto& [hash, torrentTagValues] :
					 m_torrentTags.items())
				{
					if (!torrentTagValues.is_array())
						continue;
					Json retained = Json::array();
					for (const auto& tag : torrentTagValues)
					{
						if (tag.is_string()
							&& tags.contains(tag.get<std::string>()))
						{
							retained.push_back(tag);
						}
					}
					torrentTagValues = std::move(retained);
				}
			}
			std::vector<std::string> sorted(tags.begin(), tags.end());
			std::ranges::sort(sorted);
			m_tags = sorted;
			PersistTorrentMetadataLocked();
			return EmptyResponse(request);
		}

		Response MutateTorrentTags(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string_view operation)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("hashes"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hashes");
			const auto hashes =
				ResolveTorrentHashes(parameters.at("hashes"));
			const auto values = parameters.contains("tags")
				? SplitValues(parameters.at("tags"), ',')
				: std::vector<std::string>{};
			const std::unordered_set<std::string> requested(
				values.begin(), values.end());

			std::scoped_lock lock(m_metadataMutex);
			std::unordered_set<std::string> globalTags;
			for (const auto& tag : m_tags)
			{
				if (tag.is_string())
					globalTags.insert(tag.get<std::string>());
			}
			if (operation != "remove")
			{
				globalTags.insert(requested.begin(), requested.end());
			}

			for (const auto& hash : hashes)
			{
				std::unordered_set<std::string> torrentTags;
				if (operation != "set"
					&& m_torrentTags.contains(hash)
					&& m_torrentTags.at(hash).is_array())
				{
					for (const auto& tag : m_torrentTags.at(hash))
					{
						if (tag.is_string())
							torrentTags.insert(tag.get<std::string>());
					}
				}
				if (operation == "remove")
				{
					if (requested.empty())
						torrentTags.clear();
					else
					{
						for (const auto& tag : requested)
							torrentTags.erase(tag);
					}
				}
				else
				{
					torrentTags.insert(requested.begin(), requested.end());
				}
				std::vector<std::string> sorted(
					torrentTags.begin(), torrentTags.end());
				std::ranges::sort(sorted);
				m_torrentTags[hash] = sorted;
			}

			std::vector<std::string> sortedGlobal(
				globalTags.begin(), globalTags.end());
			std::ranges::sort(sortedGlobal);
			m_tags = sortedGlobal;
			PersistTorrentMetadataLocked();
			return EmptyResponse(request);
		}

		Response MutateTorrentState(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string_view operation)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return TextResponse(
					request, http::status::service_unavailable,
					"Torrent core is not available");

			if (operation == "rename"
				|| operation == "renameFile"
				|| operation == "renameFolder")
			{
				if (!parameters.contains("hash"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: hash");
				const auto torrent =
					FindTorrent(parameters.at("hash"));
				if (!torrent)
					return TextResponse(
						request, http::status::not_found,
						"Not Found");

				if (operation == "rename")
				{
					if (!parameters.contains("name"))
						return TextResponse(
							request, http::status::bad_request,
							"Missing parameter: name");
					std::string name = Trim(parameters.at("name"));
					std::ranges::replace(name, '\r', ' ');
					std::ranges::replace(name, '\n', ' ');
					if (name.empty())
						return TextResponse(
							request, http::status::conflict,
							"Incorrect torrent name");
					std::scoped_lock lock(m_metadataMutex);
					m_torrentOverrides[
						parameters.at("hash")]["name"] = name;
						PersistTorrentMetadataLocked();
						return EmptyResponse(request);
				}

				if (!parameters.contains("oldPath")
					|| !parameters.contains("newPath"))
				{
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter");
				}
				const auto& oldPath = parameters.at("oldPath");
				const auto& newPath = parameters.at("newPath");
				bool renamed = false;
				for (const auto& file : torrent->second.files)
				{
					if (operation == "renameFile"
						&& file.path == oldPath)
					{
						core->RenameFile(
							torrent->first.taskId,
							file.fileIndex, newPath);
						renamed = true;
						break;
					}
					const std::string prefix = oldPath + "/";
					if (operation == "renameFolder"
						&& file.path.starts_with(prefix))
					{
						core->RenameFile(
							torrent->first.taskId,
							file.fileIndex,
							newPath + file.path.substr(
								oldPath.size()));
						renamed = true;
					}
				}
				return renamed
					? EmptyResponse(request)
					: TextResponse(
						request, http::status::conflict,
						"Path does not exist");
			}

			if (operation == "setSavePath"
				|| operation == "setDownloadPath")
			{
				if (!parameters.contains("id")
					|| !parameters.contains("path"))
				{
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter");
				}
				const auto hashes =
					ResolveTorrentHashes(parameters.at("id"));
				const auto& path = parameters.at("path");
				if (operation == "setSavePath" && path.empty())
					return TextResponse(
						request, http::status::bad_request,
						"Save path cannot be empty");
				if (!path.empty())
				{
					std::error_code filesystemError;
					std::filesystem::create_directories(
						PathFromUtf8(path),
						filesystemError);
					if (filesystemError)
						return TextResponse(
							request, http::status::conflict,
							"Cannot create target directory");
				}
				if (operation == "setSavePath")
				{
					for (const auto& taskId :
						 ResolveTaskIds(parameters.at("id")))
					{
						core->MoveStorage(taskId, path);
					}
				}
				else
				{
					std::scoped_lock lock(m_metadataMutex);
					for (const auto& hash : hashes)
					{
						m_torrentOverrides[
							hash]["download_path"] = path;
					}
					PersistTorrentMetadataLocked();
				}
				return EmptyResponse(request);
			}

			if (!parameters.contains("hashes"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hashes");
			const auto& hashesText = parameters.at("hashes");
			const auto taskIds = ResolveTaskIds(hashesText);

			if (operation == "increasePrio"
				|| operation == "decreasePrio"
				|| operation == "topPrio"
				|| operation == "bottomPrio")
			{
				for (const auto& taskId : taskIds)
					core->AdjustQueuePosition(taskId, operation);
				return EmptyResponse(request);
			}
			if (operation == "toggleSequentialDownload"
				|| operation == "toggleFirstLastPiecePrio")
			{
				for (const auto& taskId : taskIds)
				{
					if (operation == "toggleSequentialDownload")
						core->ToggleSequentialDownload(taskId);
					else
						core->ToggleFirstLastPiecePriority(taskId);
				}
				return EmptyResponse(request);
			}
			if (operation == "setForceStart"
				|| operation == "setSuperSeeding")
			{
				if (!parameters.contains("value"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: value");
				const auto enabled =
					ParseBoolean(parameters.at("value")).value_or(false);
				for (const auto& taskId : taskIds)
				{
					if (operation == "setForceStart")
						core->SetForceStart(taskId, enabled);
					else
						core->SetSuperSeeding(taskId, enabled);
				}
				return EmptyResponse(request);
			}
			if (operation == "setLocation")
			{
				if (!parameters.contains("location")
					|| Trim(parameters.at("location")).empty())
				{
					return TextResponse(
						request, http::status::bad_request,
						"Save path cannot be empty");
				}
				const std::string path =
					Trim(parameters.at("location"));
				std::error_code filesystemError;
				std::filesystem::create_directories(
					PathFromUtf8(path),
					filesystemError);
				if (filesystemError)
					return TextResponse(
						request, http::status::conflict,
						"Cannot make save path");
				for (const auto& taskId : taskIds)
					core->MoveStorage(taskId, path);
				const auto hashes =
					ResolveTorrentHashes(hashesText);
				std::scoped_lock lock(m_metadataMutex);
				for (const auto& hash : hashes)
					m_torrentOverrides[hash]["auto_tmm"] = false;
				PersistTorrentMetadataLocked();
				return EmptyResponse(request);
			}

			const auto hashes = ResolveTorrentHashes(hashesText);
			if (operation == "setAutoManagement")
			{
				if (!parameters.contains("enable"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: enable");
				const bool enabled =
					ParseBoolean(parameters.at("enable"))
					.value_or(false);
				std::scoped_lock lock(m_metadataMutex);
				for (const auto& hash : hashes)
					m_torrentOverrides[hash]["auto_tmm"] = enabled;
				PersistTorrentMetadataLocked();
				return EmptyResponse(request);
			}
			if (operation == "setComment")
			{
				if (!parameters.contains("comment"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: comment");
				const std::string comment =
					Trim(parameters.at("comment"));
				std::scoped_lock lock(m_metadataMutex);
				for (const auto& hash : hashes)
					m_torrentOverrides[hash]["comment"] = comment;
				PersistTorrentMetadataLocked();
				return EmptyResponse(request);
			}
			if (operation == "setShareLimits")
			{
				constexpr std::array<std::pair<
					std::string_view, std::string_view>, 5> Mappings{ {
					{"ratioLimit", "max_ratio"},
					{"seedingTimeLimit", "max_seeding_time"},
					{"inactiveSeedingTimeLimit",
						"max_inactive_seeding_time"},
					{"shareLimitAction", "share_limit_action"},
					{"shareLimitsMode", "share_limit_mode"}
				} };
				for (const auto& [source, destination] : Mappings)
				{
					if (!parameters.contains(std::string(source)))
						return TextResponse(
							request, http::status::bad_request,
							"Missing parameter");
				}
				double ratio = 0;
				try
				{
					ratio = std::stod(
						parameters.at("ratioLimit"));
				}
				catch (...)
				{
					return TextResponse(
						request, http::status::bad_request,
						"Invalid ratioLimit");
				}
				const auto parseInteger =
					[&parameters](const char* name)
					-> std::optional<int>
				{
					const auto& text = parameters.at(name);
					int value = 0;
					const auto [position, error] = std::from_chars(
						text.data(), text.data() + text.size(), value);
					if (error != std::errc{}
						|| position != (
							text.data() + text.size()))
					{
						return std::nullopt;
					}
					return value;
				};
				const auto seedingTime =
					parseInteger("seedingTimeLimit");
				const auto inactiveSeedingTime =
					parseInteger("inactiveSeedingTimeLimit");
				if (!seedingTime || !inactiveSeedingTime)
					return TextResponse(
						request, http::status::bad_request,
						"Invalid seeding time limit");
				std::scoped_lock lock(m_metadataMutex);
				for (const auto& hash : hashes)
				{
					auto& overrides = m_torrentOverrides[hash];
					overrides["max_ratio"] = ratio;
					overrides["max_seeding_time"] =
						*seedingTime;
					overrides["max_inactive_seeding_time"] =
						*inactiveSeedingTime;
					overrides["share_limit_action"] =
						parameters.at("shareLimitAction");
					overrides["share_limit_mode"] =
						parameters.at("shareLimitsMode");
				}
				PersistTorrentMetadataLocked();
				return EmptyResponse(request);
			}
			return TextResponse(
				request, http::status::not_found, "Not Found");
		}

		Json TorrentList()
		{
			Json result = Json::array();
			auto& manager = ::OpenNet::Core::P2PManager::Instance();
			auto* core = manager.TorrentCore();
			if (!core)
				return result;

			for (const auto& task : manager.GetAllTasks())
			{
				const auto detail = core->GetTorrentDetail(task.taskId);
				if (detail.taskId.empty())
					continue;
				auto torrent = SerializeTorrent(task, detail);
				const int downloadLimit =
					core->GetTorrentDownloadLimit(task.taskId);
				const int uploadLimit =
					core->GetTorrentUploadLimit(task.taskId);
				torrent["dl_limit"] =
					(downloadLimit == 0) ? -1 : downloadLimit;
				torrent["up_limit"] =
					(uploadLimit == 0) ? -1 : uploadLimit;
				ApplyTorrentMetadata(torrent);
				result.push_back(std::move(torrent));
			}
			return result;
		}

		Json TorrentMap()
		{
			Json result = Json::object();
			for (const auto& torrent : TorrentList())
				result[torrent.at("hash").get<std::string>()] = torrent;
			return result;
		}

		Response MainData(
			const Request& request, const std::string& sessionId)
		{
			const auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			const auto statistics = core
				? core->GetSessionStats()
				: ::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats{};
			const auto settings =
				::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			Json serverState{
				{"alltime_dl", statistics.totalDownloaded},
				{"alltime_ul", statistics.totalUploaded},
				{"average_time_queue", 0},
				{"connection_status",
					statistics.isListening ? "connected" : "disconnected"},
				{"dht_nodes", statistics.dhtNodes},
				{"dl_info_data", statistics.totalDownloaded},
				{"dl_info_speed", statistics.totalDownloadRate},
				{"dl_rate_limit", m_speedLimitsMode.load()
					? m_altDownloadLimit.load()
					: settings.downloadRateLimit},
				{"free_space_on_disk", FreeSpaceForDefaultPath()},
				{"global_ratio", statistics.totalDownloaded > 0
					? std::to_string(
						static_cast<double>(statistics.totalUploaded)
						/ statistics.totalDownloaded)
					: "-"},
				{"last_external_address_v4", ""},
				{"last_external_address_v6", ""},
				{"queueing", true},
				{"queued_io_jobs", 0},
				{"queued_tracker_announces", 0},
				{"read_cache_hits", "0"},
				{"read_cache_overload", "0"},
				{"refresh_interval", 1500},
				{"request_latency", 0},
				{"total_buffers_size", 0},
				{"total_peer_connections", statistics.numPeers},
				{"total_queued_size", 0},
				{"total_wasted_session", 0},
				{"up_info_data", statistics.totalUploaded},
				{"up_info_speed", statistics.totalUploadRate},
				{"up_rate_limit", m_speedLimitsMode.load()
					? m_altUploadLimit.load()
					: settings.uploadRateLimit},
				{"use_alt_speed_limits",
					m_speedLimitsMode.load()},
				{"write_cache_overload", "0"}
			};
			return JsonResponse(request, Json{
				{"rid", NextResponseId(sessionId)},
				{"full_update", true},
				{"torrents", TorrentMap()},
				{"categories", Categories()},
				{"tags", Tags()},
				{"trackers", Json::object()},
				{"server_state", std::move(serverState)}
								});
		}

		std::int64_t FreeSpaceForDefaultPath()
		{
			std::error_code error;
			auto path = PathFromUtf8(DefaultSavePath());
			while (!path.empty() && !std::filesystem::exists(path, error))
				path = path.parent_path();
			if (path.empty())
				return -1;
			const auto space = std::filesystem::space(path, error);
			if (error || space.available > static_cast<std::uintmax_t>(
				std::numeric_limits<std::int64_t>::max()))
			{
				return -1;
			}
			return static_cast<std::int64_t>(space.available);
		}

		std::optional<std::pair<
			::OpenNet::Core::Torrent::TaskMetadata,
			::OpenNet::Core::Torrent::LibtorrentHandle::TorrentDetailInfo>>
			FindTorrent(const std::string& hash)
		{
			auto& manager = ::OpenNet::Core::P2PManager::Instance();
			auto* core = manager.TorrentCore();
			if (!core)
				return std::nullopt;
			for (const auto& task : manager.GetAllTasks())
			{
				auto detail = core->GetTorrentDetail(task.taskId);
				if (detail.apiHash == hash
					|| detail.infoHashV1 == hash
					|| task.taskId == hash)
				{
					return std::pair{ task, std::move(detail) };
				}
			}
			return std::nullopt;
		}

		Response TorrentProperties(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (!parameters.contains("hash"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hash");
			const auto torrent = FindTorrent(parameters.at("hash"));
			if (!torrent)
				return TextResponse(
					request, http::status::not_found, "Not Found");
			const auto& [task, detail] = *torrent;
			const auto data = SerializeTorrent(task, detail);
			const std::int64_t totalSize = data.at("total_size");
			const std::int64_t completed = data.at("completed");
			const auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			const int downloadLimit = core
				? core->GetTorrentDownloadLimit(task.taskId) : 0;
			const int uploadLimit = core
				? core->GetTorrentUploadLimit(task.taskId) : 0;
			return JsonResponse(request, Json{
				{"time_elapsed", 0},
				{"seeding_time", 0},
				{"eta", data.at("eta")},
				{"nb_connections", detail.numConnections},
				{"nb_connections_limit", 0},
				{"total_downloaded", completed},
				{"total_downloaded_session", completed},
				{"total_uploaded", detail.totalUploaded},
				{"total_uploaded_session", detail.totalUploaded},
				{"dl_speed", detail.downloadRate},
				{"dl_speed_avg", detail.downloadRate},
				{"up_speed", detail.uploadRate},
				{"up_speed_avg", detail.uploadRate},
				{"dl_limit",
					(downloadLimit == 0) ? -1 : downloadLimit},
				{"up_limit",
					(uploadLimit == 0) ? -1 : uploadLimit},
				{"total_wasted", 0},
				{"seeds", detail.numSeeds},
				{"seeds_total", detail.numSeeds},
				{"peers", std::max(0, detail.numPeers - detail.numSeeds)},
				{"peers_total", std::max(0, detail.numPeers - detail.numSeeds)},
				{"share_ratio", detail.shareRatio},
				{"popularity", 0.0},
				{"availability", totalSize > 0
					? static_cast<double>(completed) / totalSize : 0.0},
				{"reannounce", 0},
				{"total_size", totalSize},
				{"pieces_num", detail.piecesNum},
				{"piece_size", detail.pieceSize},
				{"pieces_have", detail.pieceSize > 0
					? static_cast<std::int64_t>(
						completed / detail.pieceSize)
					: 0},
				{"created_by", ""},
				{"last_seen", -1},
				{"addition_date", task.addedTimestamp},
				{"completion_date", -1},
				{"creation_date", -1},
				{"save_path", detail.savePath},
				{"download_path", ""},
				{"comment", detail.comment},
				{"infohash_v1", detail.infoHashV1},
				{"infohash_v2", detail.infoHashV2},
				{"hash", detail.apiHash},
				{"name", detail.name},
				{"has_metadata", data.at("has_metadata")},
				{"progress", data.at("progress")},
				{"private", nullptr},
				{"is_private", false}
								});
		}

		Response TorrentTrackers(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (!parameters.contains("hash"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hash");
			const auto torrent = FindTorrent(parameters.at("hash"));
			if (!torrent)
				return TextResponse(
					request, http::status::not_found, "Not Found");
			Json result = Json::array({
				Json{
					{"url", "** [DHT] **"},
					{"status", 0},
					{"tier", -1},
					{"num_peers", 0},
					{"num_seeds", 0},
					{"num_leeches", 0},
					{"num_downloaded", 0},
					{"msg", ""}
				}
									  });
			for (const auto& tracker : torrent->second.trackers)
			{
				int status = 0;
				if (tracker.status == "working")
					status = 2;
				else if (tracker.status == "updating")
					status = 1;
				else if (tracker.status == "error")
					status = 4;
				result.push_back(Json{
					{"url", tracker.url},
					{"status", status},
					{"tier", tracker.tier},
					{"num_peers", tracker.numPeers},
					{"num_seeds", 0},
					{"num_leeches", 0},
					{"num_downloaded", 0},
					{"msg", tracker.message}
								 });
			}
			return JsonResponse(request, result);
		}

		Response TorrentPeers(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string& sessionId)
		{
			if (!parameters.contains("hash"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hash");
			const auto torrent = FindTorrent(parameters.at("hash"));
			if (!torrent)
				return TextResponse(
					request, http::status::not_found, "Not Found");

			Json peers = Json::object();
			auto& geo = ::OpenNet::Core::GeoIPManager::Instance();
			bool hasCountryData = false;
			for (const auto& peer : torrent->second.peers)
			{
				const bool ipv6 = peer.ip.find(':') != std::string::npos;
				const std::string address = ipv6
					? ("[" + peer.ip + "]:" + std::to_string(peer.port))
					: (peer.ip + ":" + std::to_string(peer.port));
				std::string connection = "BT";
				if (peer.connectionType == 1)
					connection = "Web";
				else if (peer.connectionType == 2)
					connection = "HTTP";
				const double contribution =
					(peer.totalUploaded > 0)
					? static_cast<double>(peer.totalUploaded)
					/ std::max<std::int64_t>(
						1, peer.totalDownloaded)
					: 0.0;
				auto countryCode = geo.LookupCountryCode(peer.ip);
				std::ranges::transform(
					countryCode,
					countryCode.begin(),
					[](unsigned char value)
				{
					return static_cast<char>(std::tolower(value));
				});
				auto const country = geo.LookupCountryName(peer.ip);
				hasCountryData = hasCountryData
					|| !countryCode.empty();
				peers[address] = Json{
					{"client", peer.client},
					{"peer_id_client", peer.client},
					{"progress", std::clamp(peer.progress, 0.0, 1.0)},
					{"dl_speed",
						static_cast<std::int64_t>(peer.downloadRateKB)
							* 1024},
					{"up_speed",
						static_cast<std::int64_t>(peer.uploadRateKB)
							* 1024},
					{"downloaded", peer.totalDownloaded},
					{"uploaded", peer.totalUploaded},
					{"connection", std::move(connection)},
					{"flags", ""},
					{"flags_desc", ""},
					{"relevance", 0.0},
					{"contribution", contribution},
					{"files", ""},
					{"ip", peer.ip},
					{"port", peer.port},
					{"host_name", ""},
					{"country_code", std::move(countryCode)},
					{"country", country}
				};
			}
			return JsonResponse(request, Json{
				{"rid", NextResponseId(sessionId)},
				{"full_update", true},
				{"show_flags", hasCountryData},
				{"peers", std::move(peers)}
								});
		}

		Response TorrentPieceData(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string_view kind)
		{
			if (!parameters.contains("hash"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hash");
			const auto torrent = FindTorrent(parameters.at("hash"));
			if (!torrent)
				return TextResponse(
					request, http::status::not_found, "Not Found");
			const auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return JsonResponse(request, Json::array());
			const auto info =
				core->GetTorrentPieceInfo(torrent->first.taskId);
			if (kind == "pieceStates")
				return JsonResponse(request, info.states);
			if (kind == "pieceAvailability")
				return JsonResponse(request, info.availability);
			if (kind == "pieceHashes")
				return JsonResponse(request, info.hashes);
			Json webSeeds = Json::array();
			for (const auto& url : info.webSeeds)
				webSeeds.push_back(Json{ {"url", url} });
			return JsonResponse(request, webSeeds);
		}

		Response TorrentFiles(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (!parameters.contains("hash"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hash");
			const auto torrent = FindTorrent(parameters.at("hash"));
			if (!torrent)
				return TextResponse(
					request, http::status::not_found, "Not Found");
			Json result = Json::array();
			const auto& files = torrent->second.files;
			for (std::size_t index = 0; index < files.size(); ++index)
			{
				const auto& file = files[index];
				Json value{
					{"index", file.fileIndex},
					{"name", file.path},
					{"size", file.size},
					{"progress", file.size > 0
						? static_cast<double>(file.bytesCompleted) / file.size
						: 0.0},
					{"priority", file.priority},
					{"piece_range", Json::array({
						file.firstPiece, file.lastPiece})}
				};
				if (index == 0)
				{
					value["is_seed"] = torrent->second.totalSize > 0
						&& torrent->second.totalDone
						>= torrent->second.totalSize;
				}
				result.push_back(std::move(value));
			}
			return JsonResponse(request, result);
		}

		Response AddTorrent(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::vector<FormPart>& multipart)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return TextResponse(
					request, http::status::service_unavailable,
					"Torrent core is not available");
			const std::string savePath =
				parameters.contains("savepath")
				&& !parameters.at("savepath").empty()
				? parameters.at("savepath")
				: DefaultSavePath();
			bool attempted = false;
			bool succeeded = false;

			const auto addUrls = [&](std::string urls)
			{
				std::ranges::replace(urls, '\r', '\n');
				std::istringstream stream(urls);
				std::string url;
				while (std::getline(stream, url))
				{
					url = Trim(url);
					if (url.empty())
						continue;
					attempted = true;
					if (url.starts_with("magnet:"))
						succeeded = core->AddMagnet(url, savePath) || succeeded;
				}
			};
			if (parameters.contains("urls"))
				addUrls(parameters.at("urls"));

			for (const auto& part : multipart)
			{
				if (part.name == "urls" && part.filename.empty())
				{
					addUrls(part.data);
					continue;
				}
				if (part.filename.empty())
					continue;

				attempted = true;
				const auto temporary = std::filesystem::temp_directory_path()
					/ PathFromUtf8(
						"opennet-" + RandomHex(12) + ".torrent");
				try
				{
					{
						std::ofstream output(temporary, std::ios::binary);
						output.write(
							part.data.data(),
							static_cast<std::streamsize>(part.data.size()));
					}
					succeeded = core->AddTorrentFile(
						PathUtf8(temporary), savePath) || succeeded;
					std::error_code error;
					std::filesystem::remove(temporary, error);
				}
				catch (...)
				{
					std::error_code error;
					std::filesystem::remove(temporary, error);
				}
			}

			if (!attempted)
				return TextResponse(
					request, http::status::bad_request,
					"No torrent URL or file was provided");
			if (!succeeded)
				return TextResponse(
					request, http::status::conflict,
					"Torrent could not be added");
			return TextResponse(request, http::status::ok, "Ok.");
		}

		std::vector<std::string> ResolveTaskIds(
			const std::string& hashes)
		{
			std::vector<std::string> result;
			auto& manager = ::OpenNet::Core::P2PManager::Instance();
			auto* core = manager.TorrentCore();
			if (!core)
				return result;
			const auto tasks = manager.GetAllTasks();
			if (hashes == "all")
			{
				for (const auto& task : tasks)
					result.push_back(task.taskId);
				return result;
			}

			std::unordered_set<std::string> requested;
			std::string_view remaining = hashes;
			while (!remaining.empty())
			{
				const auto separator = remaining.find('|');
				requested.emplace(remaining.substr(0, separator));
				if (separator == std::string_view::npos)
					break;
				remaining.remove_prefix(separator + 1);
			}
			for (const auto& task : tasks)
			{
				const auto detail = core->GetTorrentDetail(task.taskId);
				if (requested.contains(detail.apiHash)
					|| requested.contains(detail.infoHashV1)
					|| requested.contains(task.taskId))
				{
					result.push_back(task.taskId);
				}
			}
			return result;
		}

		Response TorrentAction(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string_view action)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("hashes"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hashes");
			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return TextResponse(
					request, http::status::service_unavailable,
					"Torrent core is not available");
			const auto publicHashes = (action == "delete")
				? ResolveTorrentHashes(parameters.at("hashes"))
				: std::vector<std::string>{};
			const auto taskIds = ResolveTaskIds(parameters.at("hashes"));
			const bool deleteFiles = parameters.contains("deleteFiles")
				&& (parameters.at("deleteFiles") == "true"
					|| parameters.at("deleteFiles") == "1");
			for (const auto& taskId : taskIds)
			{
				if (action == "start")
					core->ResumeTorrent(taskId);
				else if (action == "stop")
					core->PauseTorrent(taskId);
				else if (action == "delete")
					core->RemoveTorrent(taskId, deleteFiles);
				else if (action == "recheck")
					core->ForceRecheck(taskId);
				else if (action == "reannounce")
					core->ForceReannounce(taskId);
			}
			if (action == "delete" && !publicHashes.empty())
			{
				std::scoped_lock lock(m_metadataMutex);
				for (const auto& hash : publicHashes)
				{
					m_torrentCategories.erase(hash);
					m_torrentTags.erase(hash);
					m_torrentOverrides.erase(hash);
				}
				PersistTorrentMetadataLocked();
			}
			return EmptyResponse(request);
		}

		Response SetFilePriority(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("hash")
				|| !parameters.contains("id")
				|| !parameters.contains("priority"))
			{
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter");
			}
			const auto torrent = FindTorrent(parameters.at("hash"));
			if (!torrent)
				return TextResponse(
					request, http::status::not_found, "Not Found");
			std::vector<int> priorities;
			priorities.reserve(torrent->second.files.size());
			for (const auto& file : torrent->second.files)
				priorities.push_back(file.priority);

			int priority = 0;
			const auto priorityText = parameters.at("priority");
			if (priorityText == "1")
				priority = 1;
			else if (priorityText == "6")
				priority = 4;
			else if (priorityText == "7")
				priority = 7;

			std::string_view indexes = parameters.at("id");
			while (!indexes.empty())
			{
				const auto separator = indexes.find('|');
				int index = -1;
				const auto token = indexes.substr(0, separator);
				const auto [ptr, error] = std::from_chars(
					token.data(), token.data() + token.size(), index);
				if (error == std::errc{} && ptr == token.data() + token.size()
					&& index >= 0
					&& static_cast<std::size_t>(index) < priorities.size())
				{
					priorities[static_cast<std::size_t>(index)] = priority;
				}
				if (separator == std::string_view::npos)
					break;
				indexes.remove_prefix(separator + 1);
			}
			if (auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore())
			{
				core->SetFilePriorities(
					torrent->first.taskId, priorities);
			}
			return EmptyResponse(request);
		}

		Response TorrentLimits(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const bool download)
		{
			if (!parameters.contains("hashes"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hashes");
			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return JsonResponse(request, Json::object());

			Json result = Json::object();
			auto requested =
				SplitValues(parameters.at("hashes"), '|');
			if (parameters.at("hashes") == "all")
				requested = ResolveTorrentHashes("all");
			for (const auto& hash : requested)
			{
				const auto torrent = FindTorrent(hash);
				if (!torrent)
				{
					result[hash] = -1;
					continue;
				}
				const int limit = download
					? core->GetTorrentDownloadLimit(
						torrent->first.taskId)
					: core->GetTorrentUploadLimit(
						torrent->first.taskId);
				result[hash] = (limit == 0) ? -1 : limit;
			}
			return JsonResponse(request, result);
		}

		Response SetTorrentLimit(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const bool download)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("hashes")
				|| !parameters.contains("limit"))
			{
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter");
			}
			std::int64_t value = 0;
			const auto& text = parameters.at("limit");
			const auto [position, error] = std::from_chars(
				text.data(), text.data() + text.size(), value);
			if (error != std::errc{}
				|| position != (text.data() + text.size())
				|| value < 0
				|| value > std::numeric_limits<int>::max())
			{
				return TextResponse(
					request, http::status::bad_request,
					"Invalid parameter: limit");
			}
			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return TextResponse(
					request, http::status::service_unavailable,
					"Torrent core is not available");
			for (const auto& taskId :
				 ResolveTaskIds(parameters.at("hashes")))
			{
				if (download)
				{
					core->SetTorrentDownloadLimit(
						taskId, static_cast<int>(value));
				}
				else
				{
					core->SetTorrentUploadLimit(
						taskId, static_cast<int>(value));
				}
			}
			return EmptyResponse(request);
		}

		Response MutateTorrentConnections(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string_view operation)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return TextResponse(
					request, http::status::service_unavailable,
					"Torrent core is not available");

			if (operation == "addPeers")
			{
				if (!parameters.contains("hashes")
					|| !parameters.contains("peers"))
				{
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter");
				}
				const auto taskIds =
					ResolveTaskIds(parameters.at("hashes"));
				const auto peers =
					SplitValues(parameters.at("peers"), '|');
				bool validPeer = false;
				for (const auto& taskId : taskIds)
				{
					for (const auto& peer : peers)
						validPeer = core->AddPeer(taskId, peer)
						|| validPeer;
				}
				if (!validPeer)
					return TextResponse(
						request, http::status::bad_request,
						"No valid peers were specified");
				return EmptyResponse(request);
			}

			if (!parameters.contains("hash"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hash");
			const std::string hash =
				(parameters.at("hash") == "*")
				? "all" : parameters.at("hash");

			if (operation == "editTracker")
			{
				if (!parameters.contains("url")
					|| (!parameters.contains("newUrl")
						&& !parameters.contains("tier")))
				{
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter");
				}
				std::optional<int> tier;
				if (parameters.contains("tier"))
				{
					int value = 0;
					const auto& text = parameters.at("tier");
					const auto [position, error] = std::from_chars(
						text.data(), text.data() + text.size(), value);
					if (error != std::errc{}
						|| position != (text.data() + text.size())
						|| value < 0 || value > 255)
					{
						return TextResponse(
							request, http::status::bad_request,
							"tier must be between 0 and 255");
					}
					tier = value;
				}
				const std::string newUrl =
					parameters.contains("newUrl")
					? parameters.at("newUrl") : parameters.at("url");
				for (const auto& taskId : ResolveTaskIds(hash))
				{
					core->EditTracker(
						taskId, parameters.at("url"), newUrl, tier);
					core->ForceReannounce(taskId);
				}
				return EmptyResponse(request);
			}

			if (operation == "editWebSeed")
			{
				if (!parameters.contains("origUrl")
					|| !parameters.contains("newUrl"))
				{
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter");
				}
				const auto torrent = FindTorrent(hash);
				if (!torrent)
					return TextResponse(
						request, http::status::not_found,
						"Not Found");
				core->EditWebSeed(
					torrent->first.taskId,
					parameters.at("origUrl"),
					parameters.at("newUrl"));
				return EmptyResponse(request);
			}

			if (!parameters.contains("urls"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: urls");
			std::string urlText = parameters.at("urls");
			std::ranges::replace(urlText, '\r', '\n');
			auto urls = SplitValues(
				urlText,
				(operation == "addTrackers") ? '\n' : '|');
			if (operation == "addTrackers" && urls.size() == 1
				&& urls.front().find('|') != std::string::npos)
			{
				urls = SplitValues(urls.front(), '|');
			}

			const auto taskIds = ResolveTaskIds(hash);
			if (operation == "addTrackers")
			{
				for (const auto& taskId : taskIds)
					core->AddTrackers(taskId, urls);
			}
			else if (operation == "removeTrackers")
			{
				for (const auto& taskId : taskIds)
					core->RemoveTrackers(taskId, urls);
			}
			else
			{
				const auto torrent = FindTorrent(hash);
				if (!torrent)
					return TextResponse(
						request, http::status::not_found,
						"Not Found");
				if (operation == "addWebSeeds")
				{
					core->AddWebSeeds(
						torrent->first.taskId, urls);
				}
				else
				{
					core->RemoveWebSeeds(
						torrent->first.taskId, urls);
				}
			}
			return EmptyResponse(request);
		}

		Response TransferInfo(const Request& request)
		{
			const auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			const auto statistics = core
				? core->GetSessionStats()
				: ::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats{};
			const auto settings =
				::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			return JsonResponse(request, Json{
				{"dl_info_speed", statistics.totalDownloadRate},
				{"dl_info_data", statistics.totalDownloaded},
				{"up_info_speed", statistics.totalUploadRate},
				{"up_info_data", statistics.totalUploaded},
				{"dl_rate_limit", m_speedLimitsMode.load()
					? m_altDownloadLimit.load()
					: settings.downloadRateLimit},
				{"up_rate_limit", m_speedLimitsMode.load()
					? m_altUploadLimit.load()
					: settings.uploadRateLimit},
				{"dht_nodes", statistics.dhtNodes},
				{"connection_status",
					statistics.isListening ? "connected" : "disconnected"}
								});
		}

		Response SetTransferLimit(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const bool download)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			const auto item = parameters.find("limit");
			if (item == parameters.end())
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: limit");

			std::int64_t value = 0;
			const auto [position, error] = std::from_chars(
				item->second.data(),
				item->second.data() + item->second.size(),
				value);
			if (error != std::errc{}
				|| position != (item->second.data() + item->second.size())
				|| value < 0
				|| value > std::numeric_limits<int>::max())
			{
				return TextResponse(
					request, http::status::bad_request,
					"Invalid parameter: limit");
			}

			auto settings =
				::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			if (m_speedLimitsMode.load())
			{
				auto& database =
					::OpenNet::Core::AppSettingsDatabase::Instance();
				if (download)
				{
					m_altDownloadLimit.store(static_cast<int>(value));
					database.SetInt(
						"webui_transfer", "alt_download_limit", value);
				}
				else
				{
					m_altUploadLimit.store(static_cast<int>(value));
					database.SetInt(
						"webui_transfer", "alt_upload_limit", value);
				}
			}
			else if (download)
			{
				settings.downloadRateLimit = static_cast<int>(value);
			}
			else
			{
				settings.uploadRateLimit = static_cast<int>(value);
			}
			PersistAndApplyTorrentSettings(settings);
			return EmptyResponse(request);
		}

		Response GetSpeedLimits(const Request& request)
		{
			const auto settings =
				::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			return JsonResponse(request, Json{
				{"up_limit", settings.uploadRateLimit},
				{"dl_limit", settings.downloadRateLimit},
				{"alt_up_limit", m_altUploadLimit.load()},
				{"alt_dl_limit", m_altDownloadLimit.load()}
								});
		}

		Response SetSpeedLimits(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			constexpr std::array<std::string_view, 4> Names{
				"up_limit", "dl_limit",
				"alt_up_limit", "alt_dl_limit"
			};
			std::array<int, 4> values{};
			for (std::size_t index = 0; index < Names.size(); ++index)
			{
				const auto item =
					parameters.find(std::string(Names[index]));
				if (item == parameters.end())
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter");
				std::int64_t value = 0;
				const auto [position, error] = std::from_chars(
					item->second.data(),
					item->second.data() + item->second.size(),
					value);
				if (error != std::errc{}
					|| position != (
						item->second.data() + item->second.size())
					|| value < 0
					|| value > std::numeric_limits<int>::max())
				{
					return TextResponse(
						request, http::status::bad_request,
						"Invalid speed limit");
				}
				values[index] = static_cast<int>(value);
			}

			auto settings =
				::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			settings.uploadRateLimit = values[0];
			settings.downloadRateLimit = values[1];
			m_altUploadLimit.store(values[2]);
			m_altDownloadLimit.store(values[3]);
			auto& database =
				::OpenNet::Core::AppSettingsDatabase::Instance();
			database.SetInt(
				"webui_transfer", "alt_upload_limit", values[2]);
			database.SetInt(
				"webui_transfer", "alt_download_limit", values[3]);
			PersistAndApplyTorrentSettings(settings);
			return EmptyResponse(request);
		}

		Response SetSpeedLimitsMode(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const bool toggle)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			bool enabled = !m_speedLimitsMode.load();
			if (!toggle)
			{
				const auto item = parameters.find("mode");
				if (item == parameters.end())
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: mode");
				if (item->second == "1")
					enabled = true;
				else if (item->second == "0")
					enabled = false;
				else
					return TextResponse(
						request, http::status::bad_request,
						"Invalid parameter: mode");
			}
			m_speedLimitsMode.store(enabled);
			::OpenNet::Core::AppSettingsDatabase::Instance().SetBool(
				"webui_transfer", "alternative_mode", enabled);
			PersistAndApplyTorrentSettings(
				::OpenNet::Core::TorrentSettingsManager::Instance().Get());
			return EmptyResponse(request);
		}

		Response BanPeers(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters)
		{
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (!parameters.contains("peers"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: peers");
			const auto peers =
				SplitValues(parameters.at("peers"), '|');
			int added = 0;
			auto& filter =
				::OpenNet::Core::IPFilterManager::Instance();
			filter.Initialize();
			for (const auto& peer : peers)
			{
				std::string address = peer;
				if (address.starts_with('['))
				{
					const auto bracket = address.find(']');
					if (bracket != std::string::npos)
						address = address.substr(1, bracket - 1);
				}
				else
				{
					try
					{
						static_cast<void>(
							asio::ip::make_address(address));
					}
					catch (...)
					{
						const auto colon = address.rfind(':');
						if (colon != std::string::npos)
							address.resize(colon);
					}
				}

				std::string first;
				std::string last;
				if (!::OpenNet::Core::IPFilterManager::
					ParseIPOrCIDR(address, first, last))
				{
					continue;
				}
				filter.AddRule(
					first, last, 1, "Banned from qBittorrent WebAPI");
				++added;
			}
			if (added == 0)
				return TextResponse(
					request, http::status::bad_request,
					"No valid peers were specified");
			filter.ApplyToSession();
			return EmptyResponse(request);
		}

		Response TorrentExtended(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::vector<FormPart>& multipart,
			const std::string& operation)
		{
			if (operation == "parseMetadata")
			{
				if (request.method() != http::verb::post)
					return TextResponse(
						request, http::status::method_not_allowed,
						"Method Not Allowed");
				Json result = Json::array();
				for (const auto& part : multipart)
				{
					if (part.filename.empty())
						continue;
					const auto temporary =
						std::filesystem::temp_directory_path()
						/ PathFromUtf8(
							"opennet-metadata-" + RandomHex(12) + ".torrent");
					try
					{
						{
							std::ofstream output(temporary, std::ios::binary);
							output.write(
								part.data.data(),
								static_cast<std::streamsize>(part.data.size()));
						}
						const auto metadata =
							::OpenNet::Core::Torrent::TorrentMetadataFetcher::
							ParseTorrentFile(PathUtf8(temporary));
						std::error_code removeError;
						std::filesystem::remove(temporary, removeError);
						if (!metadata)
							return TextResponse(
								request, http::status::unprocessable_entity,
								"Invalid torrent metadata");
						result.push_back(SerializeMetadata(*metadata));
						std::scoped_lock lock(m_metadataFileMutex);
						m_metadataFiles[metadata->infoHash] = part.data;
					}
					catch (const std::exception& exception)
					{
						std::error_code removeError;
						std::filesystem::remove(temporary, removeError);
						return TextResponse(
							request, http::status::unprocessable_entity,
							exception.what());
					}
				}
				if (result.empty())
					return TextResponse(
						request, http::status::bad_request,
						"Must specify torrent file(s)");
				return JsonResponse(request, result);
			}

			if (operation == "fetchMetadata")
			{
				const auto source = parameters.find("source");
				if (source == parameters.end() || Trim(source->second).empty())
					return TextResponse(
						request, http::status::bad_request,
						"Must specify URI or hash");
				if (const auto torrent = FindTorrent(Trim(source->second)))
				{
					::OpenNet::Core::Torrent::TorrentMetadataInfo metadata;
					metadata.infoHash = torrent->second.apiHash;
					metadata.name = torrent->second.name;
					metadata.comment = torrent->second.comment;
					metadata.totalSize = torrent->second.totalSize;
					metadata.pieceLength = torrent->second.pieceSize;
					metadata.numPieces = torrent->second.piecesNum;
					for (const auto& file : torrent->second.files)
					{
						metadata.files.push_back({
							file.path, file.size, file.priority,
							file.priority != 0, file.fileIndex });
					}
					return JsonResponse(request, SerializeMetadata(metadata));
				}
				std::string hash = Trim(source->second);
				const auto marker = ToLower(hash).find("urn:btih:");
				if (marker != std::string::npos)
				{
					hash = hash.substr(marker + 9);
					const auto end = hash.find('&');
					if (end != std::string::npos)
						hash.resize(end);
				}
				return JsonResponse(
					request,
					Json{ {"infohash_v1", hash}, {"infohash_v2", ""} },
					http::status::accepted);
			}

			if (operation == "saveMetadata")
			{
				const auto source = parameters.find("source");
				if (source == parameters.end())
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: source");
				std::scoped_lock lock(m_metadataFileMutex);
				const auto data = m_metadataFiles.find(source->second);
				if (data == m_metadataFiles.end())
					return TextResponse(
						request, http::status::conflict,
						"Metadata is not yet available");
				return BinaryResponse(
					request, data->second, "application/x-bittorrent",
					source->second + ".torrent");
			}

			const auto hashItem = parameters.find("hash");
			if (hashItem == parameters.end())
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: hash");
			const auto torrent = FindTorrent(hashItem->second);
			if (!torrent)
				return TextResponse(
					request, http::status::not_found,
					"Torrent not found");

			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core)
				return TextResponse(
					request, http::status::service_unavailable,
					"Torrent core unavailable");

			if (operation == "export")
			{
				const auto data = core->ExportTorrentFile(
					torrent->first.taskId);
				if (data.empty())
					return TextResponse(
						request, http::status::conflict,
						"Torrent metadata not available");
				return BinaryResponse(
					request,
					std::string(
						reinterpret_cast<const char*>(data.data()),
						data.size()),
					"application/x-bittorrent",
					torrent->second.apiHash + ".torrent");
			}

			if (operation == "downloadFile")
			{
				const auto fileParameter = parameters.find("file");
				if (fileParameter == parameters.end())
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: file");
				const auto& files = torrent->second.files;
				const ::OpenNet::Core::Torrent::LibtorrentHandle::
					TorrentFileEntry* selected = nullptr;
				int index{};
				const auto parsed = std::from_chars(
					fileParameter->second.data(),
					fileParameter->second.data() + fileParameter->second.size(),
					index);
				if (parsed.ec == std::errc{}
					&& parsed.ptr
					== fileParameter->second.data()
					+ fileParameter->second.size())
				{
					for (const auto& file : files)
					{
						if (file.fileIndex == index)
						{
							selected = &file;
							break;
						}
					}
				}
				else
				{
					for (const auto& file : files)
					{
						if (file.path == fileParameter->second)
						{
							selected = &file;
							break;
						}
					}
				}
				if (!selected)
					return TextResponse(
						request, http::status::conflict,
						"Invalid file index or path");
				if (selected->bytesCompleted < selected->size)
					return TextResponse(
						request, http::status::conflict,
						"File not fully downloaded");
				constexpr std::uintmax_t MaxDownloadFile =
					256ULL * 1024ULL * 1024ULL;
				const auto root = PathFromUtf8(torrent->second.savePath);
				const auto filePath = root / PathFromUtf8(selected->path);
				std::error_code error;
				const auto size = std::filesystem::file_size(filePath, error);
				if (error || !IsPathInside(root, filePath))
					return TextResponse(
						request, http::status::not_found,
						"File not found");
				if (size > MaxDownloadFile)
					return TextResponse(
						request, http::status::payload_too_large,
						"File is too large for the embedded WebUI host");
				std::ifstream input(filePath, std::ios::binary);
				std::string body{
					std::istreambuf_iterator<char>{input},
					std::istreambuf_iterator<char>{} };
				return BinaryResponse(
					request, std::move(body), "application/octet-stream",
					PathUtf8(filePath.filename()));
			}

			if (operation == "SSLParameters")
			{
				std::scoped_lock lock(m_metadataMutex);
				const auto& value = m_torrentOverrides[
					torrent->second.apiHash]["ssl"];
					if (!value.is_object())
					{
						return JsonResponse(request, Json{
							{"ssl_certificate", ""},
							{"ssl_private_key", ""},
							{"ssl_dh_params", ""}
											});
					}
					return JsonResponse(request, value);
			}

			if (operation == "setSSLParameters")
			{
				if (request.method() != http::verb::post)
					return TextResponse(
						request, http::status::method_not_allowed,
						"Method Not Allowed");
				const std::array<std::string_view, 3> keys{
					"ssl_certificate", "ssl_private_key", "ssl_dh_params" };
				for (const auto key : keys)
				{
					if (!parameters.contains(std::string(key)))
						return TextResponse(
							request, http::status::bad_request,
							"Missing TLS parameter");
				}
				std::scoped_lock lock(m_metadataMutex);
				m_torrentOverrides[torrent->second.apiHash]["ssl"] = Json{
					{"ssl_certificate", parameters.at("ssl_certificate")},
					{"ssl_private_key", parameters.at("ssl_private_key")},
					{"ssl_dh_params", parameters.at("ssl_dh_params")}
				};
				PersistTorrentMetadataLocked();
				return EmptyResponse(request);
			}

			return TextResponse(
				request, http::status::not_found, "Not Found");
		}

		Response RssApi(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string& operation)
		{
			auto& manager = ::OpenNet::Core::RSS::RSSManager::Instance();
			const auto findFeed = [&](const std::string& path)
				-> std::optional<::OpenNet::Core::RSS::RSSFeed>
			{
				const auto widePath = Utf8ToWide(path);
				for (const auto& feed : manager.GetAllFeeds())
				{
					if (feed.id == widePath || feed.title == widePath
						|| feed.url == widePath)
					{
						return feed;
					}
				}
				return std::nullopt;
			};
			const auto toSubscription = [](
				const ::OpenNet::Core::RSS::RSSFeed& feed)
			{
				::OpenNet::Core::RSS::RSSSubscription subscription;
				subscription.id = feed.id;
				subscription.url = feed.url;
				subscription.name = feed.title;
				subscription.savePath = feed.savePath;
				subscription.updateInterval = feed.updateInterval;
				subscription.autoDownload = feed.autoDownload;
				subscription.filterPattern = feed.filterPattern;
				subscription.enabled = feed.enabled;
				return subscription;
			};

			if (operation == "items")
			{
				const bool withData = parameters.contains("withData")
					&& ParseBool(parameters.at("withData"));
				Json result = Json::object();
				for (const auto& feed : manager.GetAllFeeds())
				{
					Json value{
						{"uid", WideToUtf8(feed.id)},
						{"url", WideToUtf8(feed.url)},
						{"status", "default"},
						{"hasError", false}
					};
					if (withData)
					{
						Json articles = Json::array();
						for (const auto& item : feed.items)
						{
							articles.push_back(Json{
								{"id", WideToUtf8(item.guid)},
								{"title", WideToUtf8(item.title)},
								{"date", std::chrono::duration_cast<
									std::chrono::seconds>(
									item.pubDate.time_since_epoch()).count()},
								{"author", ""},
								{"description", WideToUtf8(item.description)},
								{"link", WideToUtf8(item.link)},
								{"torrentURL", WideToUtf8(item.enclosureUrl)},
								{"isRead", item.isDownloaded}
											   });
						}
						value["articles"] = std::move(articles);
					}
					std::string name = WideToUtf8(feed.title);
					if (name.empty())
						name = WideToUtf8(feed.url);
					result[name] = std::move(value);
				}
				return JsonResponse(request, result);
			}

			if (operation == "rules")
			{
				std::scoped_lock lock(m_rssMutex);
				return JsonResponse(request, m_rssRules);
			}
			if (operation == "matchingArticles")
				return JsonResponse(request, Json::object());

			if (request.method() != http::verb::post
				&& operation != "setFeedRefreshInterval")
			{
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			}

			if (operation == "addFolder")
			{
				if (!parameters.contains("path"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: path");
				std::scoped_lock lock(m_rssMutex);
				m_rssFolders.insert(parameters.at("path"));
				return EmptyResponse(request);
			}
			if (operation == "addFeed")
			{
				if (!parameters.contains("url") || !parameters.contains("path"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing feed parameter");
				::OpenNet::Core::RSS::RSSSubscription subscription;
				subscription.id = Utf8ToWide(RandomHex(16));
				subscription.url = Utf8ToWide(parameters.at("url"));
				subscription.name = Utf8ToWide(
					parameters.at("path").empty()
					? parameters.at("url") : parameters.at("path"));
				if (parameters.contains("refreshInterval"))
				{
					const auto seconds = ParseInt64(
						parameters.at("refreshInterval")).value_or(1800);
					subscription.updateInterval = std::chrono::minutes(
						std::max<std::int64_t>(1, seconds / 60));
				}
				if (!manager.AddSubscription(subscription))
					return TextResponse(
						request, http::status::conflict,
						"Unable to add RSS feed");
				return EmptyResponse(request);
			}
			if (operation == "removeItem")
			{
				if (!parameters.contains("path"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: path");
				const auto feed = findFeed(parameters.at("path"));
				if (feed)
					manager.RemoveSubscription(feed->id);
				else
				{
					std::scoped_lock lock(m_rssMutex);
					m_rssFolders.erase(parameters.at("path"));
				}
				return EmptyResponse(request);
			}
			if (operation == "refreshItem")
			{
				if (!parameters.contains("itemPath"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: itemPath");
				if (const auto feed = findFeed(parameters.at("itemPath")))
					manager.RefreshFeed(feed->id);
				return EmptyResponse(request);
			}
			if (operation == "markAsRead")
			{
				if (!parameters.contains("itemPath"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: itemPath");
				if (const auto feed = findFeed(parameters.at("itemPath")))
				{
					if (parameters.contains("articleId"))
					{
						manager.MarkItemAsDownloaded(
							feed->id, Utf8ToWide(parameters.at("articleId")));
					}
					else
					{
						for (const auto& item : feed->items)
							manager.MarkItemAsDownloaded(feed->id, item.guid);
					}
				}
				return EmptyResponse(request);
			}
			if (operation == "setFeedURL"
				|| operation == "setFeedRefreshInterval"
				|| operation == "moveItem")
			{
				const std::string sourcePath = operation == "moveItem"
					? parameters.contains("itemPath")
					? parameters.at("itemPath") : std::string{}
					: parameters.contains("path")
					? parameters.at("path") : std::string{};
				const auto feed = findFeed(sourcePath);
				if (!feed)
					return TextResponse(
						request, http::status::conflict,
						"Feed does not exist");
				auto subscription = toSubscription(*feed);
				if (operation == "setFeedURL")
				{
					if (!parameters.contains("url"))
						return TextResponse(
							request, http::status::bad_request,
							"Missing parameter: url");
					subscription.url = Utf8ToWide(parameters.at("url"));
				}
				else if (operation == "setFeedRefreshInterval")
				{
					if (!parameters.contains("refreshInterval"))
						return TextResponse(
							request, http::status::bad_request,
							"Missing refresh interval");
					const auto seconds = ParseInt64(
						parameters.at("refreshInterval")).value_or(-1);
					if (seconds < 0)
						return TextResponse(
							request, http::status::bad_request,
							"Invalid refresh interval");
					subscription.updateInterval = std::chrono::minutes(
						std::max<std::int64_t>(1, seconds / 60));
				}
				else
				{
					if (!parameters.contains("destPath"))
						return TextResponse(
							request, http::status::bad_request,
							"Missing parameter: destPath");
					subscription.name = Utf8ToWide(parameters.at("destPath"));
				}
				if (!manager.UpdateSubscription(subscription))
					return TextResponse(
						request, http::status::conflict,
						"Unable to update RSS feed");
				return EmptyResponse(request);
			}
			if (operation == "setRule")
			{
				if (!parameters.contains("ruleName")
					|| !parameters.contains("ruleDef"))
				{
					return TextResponse(
						request, http::status::bad_request,
						"Missing rule parameter");
				}
				try
				{
					const auto definition =
						Json::parse(parameters.at("ruleDef"));
					std::scoped_lock lock(m_rssMutex);
					m_rssRules[parameters.at("ruleName")] = definition;
					PersistRssRulesLocked();
					return EmptyResponse(request);
				}
				catch (const Json::exception& exception)
				{
					return TextResponse(
						request, http::status::bad_request,
						exception.what());
				}
			}
			if (operation == "removeRule"
				|| operation == "renameRule"
				|| operation == "cloneRule")
			{
				std::scoped_lock lock(m_rssMutex);
				if (operation == "removeRule")
				{
					if (parameters.contains("ruleName"))
						m_rssRules.erase(parameters.at("ruleName"));
				}
				else if (operation == "renameRule")
				{
					if (!parameters.contains("ruleName")
						|| !parameters.contains("newRuleName"))
					{
						return TextResponse(
							request, http::status::bad_request,
							"Missing rule parameter");
					}
					const auto source =
						m_rssRules.find(parameters.at("ruleName"));
					if (source != m_rssRules.end())
					{
						m_rssRules[parameters.at("newRuleName")] = *source;
						m_rssRules.erase(source);
					}
				}
				else
				{
					if (!parameters.contains("sourceName")
						|| !parameters.contains("cloneName"))
					{
						return TextResponse(
							request, http::status::bad_request,
							"Missing rule parameter");
					}
					const auto source =
						m_rssRules.find(parameters.at("sourceName"));
					if (source != m_rssRules.end())
						m_rssRules[parameters.at("cloneName")] = *source;
				}
				PersistRssRulesLocked();
				return EmptyResponse(request);
			}
			return EmptyResponse(request);
		}

		Response SearchApi(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string& operation)
		{
			if (operation == "plugins")
				return JsonResponse(request, Json::array());
			if (operation == "status")
			{
				Json result = Json::array();
				const int requested = parameters.contains("id")
					? static_cast<int>(
						ParseInt64(parameters.at("id")).value_or(0))
					: 0;
				std::scoped_lock lock(m_searchMutex);
				for (const auto& [id, job] : m_searchJobs)
				{
					if (requested != 0 && requested != id)
						continue;
					result.push_back(Json{
						{"id", id},
						{"status", job.running ? "Running" : "Stopped"},
						{"total", job.results.size()}
									 });
				}
				return JsonResponse(request, result);
			}
			if (operation == "results")
			{
				if (!parameters.contains("id"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: id");
				const int id = static_cast<int>(
					ParseInt64(parameters.at("id")).value_or(0));
				std::scoped_lock lock(m_searchMutex);
				const auto job = m_searchJobs.find(id);
				if (job == m_searchJobs.end())
					return TextResponse(
						request, http::status::not_found,
						"Search job not found");
				return JsonResponse(request, Json{
					{"status", job->second.running ? "Running" : "Stopped"},
					{"results", job->second.results},
					{"total", job->second.results.size()}
									});
			}
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (operation == "start")
			{
				if (!parameters.contains("pattern")
					|| !parameters.contains("category")
					|| !parameters.contains("plugins"))
				{
					return TextResponse(
						request, http::status::bad_request,
						"Missing search parameter");
				}
				const int id = m_nextSearchId.fetch_add(1);
				std::scoped_lock lock(m_searchMutex);
				m_searchJobs.emplace(id, SearchJob{ id, false, Json::array() });
				return JsonResponse(request, Json{ {"id", id} });
			}
			if (operation == "downloadTorrent")
			{
				if (!parameters.contains("torrentUrl"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: torrentUrl");
				const auto& source = parameters.at("torrentUrl");
				if (!ToLower(source).starts_with("magnet:"))
					return TextResponse(
						request, http::status::conflict,
						"Only magnet search results are supported");
				auto* core =
					::OpenNet::Core::P2PManager::Instance().TorrentCore();
				if (!core || !core->AddMagnet(source, DefaultSavePath()))
					return TextResponse(
						request, http::status::conflict,
						"Unable to add search result");
				return EmptyResponse(request);
			}
			if (operation == "installPlugin"
				|| operation == "uninstallPlugin"
				|| operation == "enablePlugin"
				|| operation == "updatePlugins")
			{
				return TextResponse(
					request, http::status::conflict,
					"External Python search plugins are not supported");
			}
			if (!parameters.contains("id"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: id");
			const int id = static_cast<int>(
				ParseInt64(parameters.at("id")).value_or(0));
			std::scoped_lock lock(m_searchMutex);
			const auto job = m_searchJobs.find(id);
			if (job == m_searchJobs.end())
				return TextResponse(
					request, http::status::not_found,
					"Search job not found");
			if (operation == "delete")
				m_searchJobs.erase(job);
			else if (operation == "stop")
				job->second.running = false;
			return EmptyResponse(request);
		}

		Response TorrentCreatorApi(
			const Request& request,
			const std::unordered_map<std::string, std::string>& parameters,
			const std::string& operation)
		{
			if (operation == "status")
			{
				Json result = Json::array();
				std::scoped_lock lock(m_creatorMutex);
				if (parameters.contains("taskID")
					&& !parameters.at("taskID").empty())
				{
					const auto task =
						m_creatorJobs.find(parameters.at("taskID"));
					if (task == m_creatorJobs.end())
						return TextResponse(
							request, http::status::not_found,
							"Creation task not found");
					result.push_back(task->second.status);
				}
				else
				{
					for (const auto& [id, task] : m_creatorJobs)
						result.push_back(task.status);
				}
				return JsonResponse(request, result);
			}
			if (operation == "torrentFile")
			{
				if (!parameters.contains("taskID"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: taskID");
				std::scoped_lock lock(m_creatorMutex);
				const auto task =
					m_creatorJobs.find(parameters.at("taskID"));
				if (task == m_creatorJobs.end())
					return TextResponse(
						request, http::status::not_found,
						"Creation task not found");
				if (task->second.torrentData.empty())
					return TextResponse(
						request, http::status::conflict,
						"Torrent creation failed");
				return BinaryResponse(
					request, task->second.torrentData,
					"application/x-bittorrent",
					parameters.at("taskID") + ".torrent");
			}
			if (request.method() != http::verb::post)
				return TextResponse(
					request, http::status::method_not_allowed,
					"Method Not Allowed");
			if (operation == "deleteTask")
			{
				if (!parameters.contains("taskID"))
					return TextResponse(
						request, http::status::bad_request,
						"Missing parameter: taskID");
				std::scoped_lock lock(m_creatorMutex);
				if (m_creatorJobs.erase(parameters.at("taskID")) == 0)
					return TextResponse(
						request, http::status::not_found,
						"Creation task not found");
				return EmptyResponse(request);
			}
			if (operation != "addTask" || !parameters.contains("sourcePath"))
				return TextResponse(
					request, http::status::bad_request,
					"Missing parameter: sourcePath");

			const auto sourcePath =
				PathFromUtf8(parameters.at("sourcePath"));
			std::error_code filesystemError;
			if (!std::filesystem::exists(sourcePath, filesystemError))
				return TextResponse(
					request, http::status::conflict,
					"Source path does not exist");
			const std::string taskId = RandomHex(16);
			CreatorJob job;
			const auto now = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			job.status = Json{
				{"taskID", taskId},
				{"sourcePath", parameters.at("sourcePath")},
				{"pieceSize", 0},
				{"ignoreDotFiles", true},
				{"private", false},
				{"format", parameters.contains("format")
					? parameters.at("format") : "hybrid"},
				{"timeAdded", now},
				{"status", "Running"},
				{"progress", 0.0}
			};
			try
			{
				libtorrent::file_storage storage;
				const std::string source = PathUtf8(sourcePath);
				libtorrent::add_files(storage, source);
				if (storage.num_files() == 0)
					throw std::runtime_error("No files found");
				int pieceSize = parameters.contains("pieceSize")
					? static_cast<int>(
						ParseInt64(parameters.at("pieceSize")).value_or(0))
					: 0;
				libtorrent::create_flags_t flags{};
				const std::string format = parameters.contains("format")
					? ToLower(parameters.at("format")) : "hybrid";
				if (format == "v1")
					flags |= libtorrent::create_torrent::v1_only;
				else if (format == "v2")
					flags |= libtorrent::create_torrent::v2_only;
				libtorrent::create_torrent creator(storage, pieceSize, flags);
				if (parameters.contains("private"))
					creator.set_priv(ParseBool(parameters.at("private")));
				if (parameters.contains("comment"))
					creator.set_comment(parameters.at("comment").c_str());
				creator.set_creator("OpenNet");
				if (parameters.contains("trackers"))
				{
					int tier = 0;
					for (const auto& tracker :
						 SplitValues(parameters.at("trackers"), '|'))
					{
						if (!tracker.empty())
							creator.add_tracker(tracker, tier++);
					}
				}
				if (parameters.contains("urlSeeds"))
				{
					for (const auto& seed :
						 SplitValues(parameters.at("urlSeeds"), '|'))
					{
						if (!seed.empty())
							creator.add_url_seed(seed);
					}
				}
				const auto base = std::filesystem::is_directory(sourcePath)
					? sourcePath.parent_path() : sourcePath.parent_path();
				libtorrent::error_code error;
				libtorrent::set_piece_hashes(
					creator, PathUtf8(base), error);
				if (error)
					throw std::runtime_error(error.message());
				std::vector<char> encoded;
				libtorrent::bencode(
					std::back_inserter(encoded), creator.generate());
				job.torrentData.assign(encoded.begin(), encoded.end());
				job.status["pieceSize"] = creator.piece_length();
				job.status["status"] = "Finished";
				job.status["progress"] = 1.0;
				job.status["timeFinished"] =
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
						.time_since_epoch()).count();
			}
			catch (const std::exception& exception)
			{
				job.status["status"] = "Failed";
				job.status["errorMessage"] = exception.what();
			}
			{
				std::scoped_lock lock(m_creatorMutex);
				m_creatorJobs[taskId] = std::move(job);
			}
			return JsonResponse(request, Json{ {"taskID", taskId} });
		}

		void PersistRssRulesLocked()
		{
			::OpenNet::Core::AppSettingsDatabase::Instance().SetString(
				"webui_rss", "rules", m_rssRules.dump());
		}

		void LoadRssRules()
		{
			const auto stored =
				::OpenNet::Core::AppSettingsDatabase::Instance().GetString(
					"webui_rss", "rules");
			if (!stored)
				return;
			try
			{
				const auto value = Json::parse(*stored);
				if (value.is_object())
					m_rssRules = value;
			}
			catch (...)
			{
				m_rssRules = Json::object();
			}
		}

		void PersistAndApplyTorrentSettings(
			const ::OpenNet::Core::TorrentSettings& settings)
		{
			::OpenNet::Core::TorrentSettingsManager::Instance().Set(settings);
			auto* core =
				::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core || !core->IsRunning())
				return;
			auto liveSettings = settings;
			if (m_speedLimitsMode.load())
			{
				liveSettings.downloadRateLimit =
					m_altDownloadLimit.load();
				liveSettings.uploadRateLimit =
					m_altUploadLimit.load();
			}
			libtorrent::settings_pack pack;
			::OpenNet::Core::ApplyTorrentSettingsToSettingsPack(
				liveSettings, pack);
			core->ApplySettings(pack);
		}

		mutable std::mutex m_stateMutex;
		std::atomic<bool> m_running{ false };
		WebUIOptions m_options;
		std::filesystem::path m_assetRoot;
		std::string m_cacheId;
		std::string m_languageOptions;
		std::unique_ptr<asio::io_context> m_context;
		std::shared_ptr<Listener> m_listener;
		std::vector<std::thread> m_threads;

		mutable std::mutex m_sessionMutex;
		std::unordered_map<std::string, SessionState> m_sessions;

		std::mutex m_loginMutex;
		std::unordered_map<std::string, FailedLogin> m_failedLogins;

		std::mutex m_authMutex;
		std::string m_apiKey;

		std::mutex m_cookieMutex;
		Json m_cookies = Json::array();

		std::atomic<bool> m_speedLimitsMode{ false };
		std::atomic<int> m_altDownloadLimit{ 10 * 1024 };
		std::atomic<int> m_altUploadLimit{ 10 * 1024 };

		std::mutex m_clientDataMutex;
		Json m_clientData = Json::object();

		std::mutex m_preferenceMutex;
		Json m_preferenceOverrides = Json::object();

		std::mutex m_metadataMutex;
		Json m_categories = Json::object();
		Json m_tags = Json::array();
		Json m_torrentCategories = Json::object();
		Json m_torrentTags = Json::object();
		Json m_torrentOverrides = Json::object();

		std::mutex m_metadataFileMutex;
		std::unordered_map<std::string, std::string> m_metadataFiles;

		std::mutex m_rssMutex;
		Json m_rssRules = Json::object();
		std::unordered_set<std::string> m_rssFolders;

		std::mutex m_searchMutex;
		std::unordered_map<int, SearchJob> m_searchJobs;
		std::atomic<int> m_nextSearchId{ 1 };

		std::mutex m_creatorMutex;
		std::unordered_map<std::string, CreatorJob> m_creatorJobs;

		const std::chrono::system_clock::time_point m_launchTime =
			std::chrono::system_clock::now();
	};

	WebUIHost& WebUIHost::Instance()
	{
		static WebUIHost instance;
		return instance;
	}

	WebUIHost::WebUIHost()
		: m_impl(std::make_unique<Impl>())
	{
	}

	WebUIHost::~WebUIHost()
	{
		Stop();
	}

	bool WebUIHost::Start(WebUIOptions options)
	{
		return m_impl->Start(std::move(options));
	}

	void WebUIHost::Stop() noexcept
	{
		m_impl->Stop();
	}

	bool WebUIHost::Restart()
	{
		return m_impl->Restart();
	}

	bool WebUIHost::IsRunning() const noexcept
	{
		return m_impl->IsRunning();
	}

	std::uint16_t WebUIHost::Port() const noexcept
	{
		return m_impl->Port();
	}

	std::filesystem::path WebUIHost::AssetRoot() const
	{
		return m_impl->AssetRoot();
	}

	bool IsWebUIRunning() noexcept
	{
		return WebUIHost::Instance().IsRunning();
	}

	bool StartWebUI()
	{
		return WebUIHost::Instance().Start();
	}

	bool RestartWebUI()
	{
		return WebUIHost::Instance().Restart();
	}
}
