module;

#include <Windows.h>

module OpenNet.Core.Content.ContentDirectoryClient;

import OpenNet.Web.ServerDomain;
import winrt.Windows.Data.Json;
import winrt.Windows.Foundation;
import winrt.Windows.Storage.Streams;
import winrt.Windows.Web.Http;
import std;

using namespace winrt;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;

namespace OpenNet::Core::Content
{
	namespace
	{
		std::wstring ApiUri(std::wstring_view relative)
		{
			auto root = ::OpenNet::Web::ServerDomain::GetApiRoot();
			while (!root.empty() && root.back() == L'/') root.pop_back();
			std::wstring result = root;
			result.append(relative);
			return result;
		}

		constexpr auto DirectoryInitializationTimeout =
			std::chrono::seconds(6);
		constexpr auto DirectoryRequestTimeout =
			std::chrono::seconds(15);
		constexpr auto DirectoryPollInterval =
			std::chrono::milliseconds(25);

		struct RegistrationTrace
		{
			inline static std::atomic<std::uint64_t> nextId{ 0 };
			std::uint64_t const id =
				nextId.fetch_add(1, std::memory_order_relaxed) + 1;
			std::chrono::steady_clock::time_point const started =
				std::chrono::steady_clock::now();
			std::wstring_view stage = L"initialize-server-domain";

			template<typename... TArgs>
			void Log(std::wformat_string<TArgs...> format, TArgs&&... args) const noexcept
			{
				try
				{
					auto const elapsed = std::chrono::duration_cast<
						std::chrono::milliseconds>(
							std::chrono::steady_clock::now() - started).count();
					auto const message = std::format(
						L"[OpenNet.ContentDirectory.Register #{}] stage={} elapsed={}ms {}\n",
						id, stage, elapsed,
						std::format(format, std::forward<TArgs>(args)...));
					OutputDebugStringW(message.c_str());
				}
				catch (...)
				{
					// Diagnostics must not affect registration or its retry behavior.
				}
			}

			void LogException(std::exception const& error) const noexcept
			{
				try
				{
					Log(L"exception={}", winrt::to_hstring(error.what()).c_str());
				}
				catch (...)
				{
					Log(L"std::exception (message conversion failed)");
				}
			}
		};

		template<typename TAsync>
		bool WaitForCompletion(
			TAsync const& operation,
			std::stop_token const& stopToken,
			std::chrono::steady_clock::duration const timeout,
			RegistrationTrace const* trace = nullptr)
		{
			auto const deadline =
				std::chrono::steady_clock::now() + timeout;
			for (;;)
			{
				if (stopToken.stop_requested())
				{
					if (trace) trace->Log(L"canceled by stop request");
					operation.Cancel();
					return false;
				}

				auto const status = operation.Status();
				if (status == AsyncStatus::Completed)
					return true;
				if (status != AsyncStatus::Started)
				{
					if (trace)
					{
						trace->Log(L"async stopped: status={} HRESULT=0x{:08X}",
								   static_cast<int>(status),
								   static_cast<std::uint32_t>(operation.ErrorCode().value));
					}
					return false;
				}
				if (std::chrono::steady_clock::now() >= deadline)
				{
					if (trace)
					{
						trace->Log(L"timeout after {}ms",
								   std::chrono::duration_cast<
								   std::chrono::milliseconds>(timeout).count());
					}
					operation.Cancel();
					return false;
				}

				std::this_thread::sleep_for(DirectoryPollInterval);
			}
		}

		bool InitializeServerDomain(
			std::stop_token const& stopToken,
			RegistrationTrace const* trace = nullptr)
		{
			auto operation =
				::OpenNet::Web::ServerDomain::InitializeAsync();
			if (!WaitForCompletion(
				operation,
				stopToken,
				DirectoryInitializationTimeout,
				trace))
				return false;
			operation.GetResults();
			return true;
		}

		JsonObject IdentityJson(ContentIdentity const& identity)
		{
			JsonObject json;
			json.Insert(L"algorithm", JsonValue::CreateNumberValue(
				static_cast<double>(static_cast<std::uint8_t>(identity.algorithm))));
			json.Insert(L"digest", JsonValue::CreateStringValue(
				winrt::to_hstring(identity.ToHex())));
			return json;
		}

		std::optional<ContentIdentity> ParseIdentity(JsonObject const& json)
		{
			auto const algorithmValue = static_cast<int>(
				json.GetNamedNumber(L"algorithm", 0));
			auto const digestValue = winrt::to_string(
				json.GetNamedString(L"digest", L""));
			if (algorithmValue <= 0 || algorithmValue > 255
				|| digestValue.size() % 2 != 0)
				return std::nullopt;

			ContentIdentity identity;
			identity.algorithm = static_cast<ContentIdentityAlgorithm>(
				static_cast<std::uint8_t>(algorithmValue));
			identity.digest.reserve(digestValue.size() / 2);

			auto hex = [](char value) -> int
			{
				if (value >= '0' && value <= '9') return value - '0';
				if (value >= 'a' && value <= 'f') return value - 'a' + 10;
				if (value >= 'A' && value <= 'F') return value - 'A' + 10;
				return -1;
			};

			for (std::size_t i = 0; i < digestValue.size(); i += 2)
			{
				int const high = hex(digestValue[i]);
				int const low = hex(digestValue[i + 1]);
				if (high < 0 || low < 0) return std::nullopt;
				identity.digest.push_back(static_cast<std::uint8_t>(
					(high << 4) | low));
			}
			return identity.IsWellFormed()
				? std::optional<ContentIdentity>{ std::move(identity) }
			: std::nullopt;
		}

		ContentPeerTransport ParseTransport(hstring const& value)
		{
			if (_wcsicmp(value.c_str(), L"Utp") == 0)
				return ContentPeerTransport::Utp;
			return ContentPeerTransport::Tcp;
		}

		std::string Base64(std::span<std::uint8_t const> bytes)
		{
			static constexpr char table[] =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			std::string result;
			result.reserve(((bytes.size() + 2) / 3) * 4);
			for (std::size_t i = 0; i < bytes.size(); i += 3)
			{
				std::uint32_t value = static_cast<std::uint32_t>(bytes[i]) << 16;
				if (i + 1 < bytes.size())
					value |= static_cast<std::uint32_t>(bytes[i + 1]) << 8;
				if (i + 2 < bytes.size())
					value |= bytes[i + 2];

				result.push_back(table[(value >> 18) & 0x3f]);
				result.push_back(table[(value >> 12) & 0x3f]);
				result.push_back(i + 1 < bytes.size()
								 ? table[(value >> 6) & 0x3f] : '=');
				result.push_back(i + 2 < bytes.size()
								 ? table[value & 0x3f] : '=');
			}
			return result;
		}

		HttpClient CreateClient()
		{
			HttpClient http;
			http.DefaultRequestHeaders().UserAgent().ParseAdd(
				L"OpenNet/1.0 ContentDirectory");
			return http;
		}
	}

	std::optional<ContentDirectoryLease> ContentDirectoryClient::Register(
		std::string const& nodeId,
		std::uint64_t generation,
		std::string const& registrationId,
		std::optional<std::string> const& previousLeaseId,
		std::vector<ContentRecord> const& contents,
		std::vector<ContentDirectoryEndpoint> const& endpoints,
		std::stop_token stopToken)
	{
		RegistrationTrace trace;
		trace.Log(L"begin generation={} contents={} endpoints={} previousLease={}",
				  generation, contents.size(), endpoints.size(),
				  previousLeaseId && !previousLeaseId->empty());
		try
		{
			if (!InitializeServerDomain(stopToken, &trace))
				return std::nullopt;

			trace.stage = L"build-request";
			JsonObject request;
			request.Insert(L"nodeId", JsonValue::CreateStringValue(
				winrt::to_hstring(nodeId)));
			request.Insert(L"generation", JsonValue::CreateNumberValue(
				static_cast<double>(generation)));
			request.Insert(L"registrationId", JsonValue::CreateStringValue(
				winrt::to_hstring(registrationId)));
			if (previousLeaseId && !previousLeaseId->empty())
			{
				request.Insert(L"previousLeaseId", JsonValue::CreateStringValue(
					winrt::to_hstring(*previousLeaseId)));
			}

			JsonArray endpointArray;
			for (auto const& endpoint : endpoints)
			{
				if (endpoint.port == 0) continue;
				JsonObject value;
				value.Insert(L"transport", JsonValue::CreateStringValue(
					endpoint.transport == ContentPeerTransport::Utp
					? L"Utp" : L"Tcp"));
				value.Insert(L"addressFamily", JsonValue::CreateStringValue(
					endpoint.addressFamily == ContentAddressFamily::Ipv6
					? L"Ipv6" : L"Ipv4"));
				value.Insert(L"port", JsonValue::CreateNumberValue(endpoint.port));
				endpointArray.Append(value);
			}
			request.Insert(L"endpoints", endpointArray);

			JsonArray contentArray;
			for (auto const& record : contents)
			{
				if (!record.IsAvailable() || record.identities.empty()) continue;
				JsonObject item;
				item.Insert(L"size", JsonValue::CreateNumberValue(
					static_cast<double>(record.size)));
				JsonArray identities;
				for (auto const& identity : record.identities)
				{
					if (identity.IsWellFormed())
						identities.Append(IdentityJson(identity));
				}
				if (identities.Size() == 0) continue;
				item.Insert(L"identities", identities);
				contentArray.Append(item);
			}
			request.Insert(L"contents", contentArray);

			auto http = CreateClient();
			HttpStringContent body{
				request.Stringify(),
				UnicodeEncoding::Utf8,
				L"application/json"
			};
			auto const uri = ApiUri(L"/api/v1/content/nodes/register");
			trace.stage = L"send";
			trace.Log(L"POST {} generation={} contents={} endpoints={}",
					  uri, generation, contentArray.Size(), endpointArray.Size());
			auto requestOperation = http.PostAsync(
				Uri{ uri },
				body);
			if (!WaitForCompletion(
				requestOperation,
				stopToken,
				DirectoryRequestTimeout,
				&trace))
				return std::nullopt;
			auto response = requestOperation.GetResults();
			trace.Log(L"HTTP {} {}", static_cast<int>(response.StatusCode()),
					  response.ReasonPhrase().c_str());
			if (!response.IsSuccessStatusCode()) return std::nullopt;

			trace.stage = L"read-response";
			auto readOperation =
				response.Content().ReadAsStringAsync();
			if (!WaitForCompletion(
				readOperation,
				stopToken,
				DirectoryRequestTimeout,
				&trace))
				return std::nullopt;
			trace.stage = L"parse-response";
			JsonObject json = JsonObject::Parse(
				readOperation.GetResults());
			ContentDirectoryLease lease;
			lease.leaseId = winrt::to_string(
				json.GetNamedString(L"leaseId"));
			lease.acceptedGeneration = static_cast<std::uint64_t>(
				json.GetNamedNumber(L"acceptedGeneration"));
			lease.contentCount = static_cast<std::size_t>(
				json.GetNamedNumber(L"contentCount"));
			trace.Log(L"success acceptedGeneration={} contentCount={}",
					  lease.acceptedGeneration, lease.contentCount);
			return lease;
		}
		catch (winrt::hresult_error const& error)
		{
			trace.Log(L"exception HRESULT=0x{:08X} {}",
					  static_cast<std::uint32_t>(error.code().value), error.message().c_str());
			return std::nullopt;
		}
		catch (std::exception const& error)
		{
			trace.LogException(error);
			return std::nullopt;
		}
		catch (...)
		{
			trace.Log(L"unknown exception");
			return std::nullopt;
		}
	}

	bool ContentDirectoryClient::Heartbeat(
		std::string const& nodeId,
		std::string const& leaseId,
		std::stop_token stopToken)
	{
		try
		{
			if (!InitializeServerDomain(stopToken))
				return false;
			JsonObject request;
			request.Insert(L"leaseId", JsonValue::CreateStringValue(
				winrt::to_hstring(leaseId)));

			auto http = CreateClient();
			HttpStringContent body{
				request.Stringify(),
				UnicodeEncoding::Utf8,
				L"application/json"
			};
			std::wstring relative = L"/api/v1/content/nodes/";
			relative += winrt::to_hstring(nodeId).c_str();
			relative += L"/heartbeat";
			auto operation =
				http.PostAsync(Uri{ ApiUri(relative) }, body);
			if (!WaitForCompletion(
				operation,
				stopToken,
				DirectoryRequestTimeout))
				return false;
			return operation.GetResults().IsSuccessStatusCode();
		}
		catch (...)
		{
			return false;
		}
	}

	std::optional<std::vector<ContentWakeup>>
		ContentDirectoryClient::PollWakeups(
			std::string const& nodeId,
			std::string const& leaseId,
			std::uint32_t maxItems,
			std::stop_token stopToken)
	{
		try
		{
			if (!InitializeServerDomain(stopToken))
				return std::nullopt;
			std::wstring relative = std::format(
				L"/api/v1/content/nodes/{}/wakeups?leaseId={}&maxItems={}",
				winrt::to_hstring(nodeId).c_str(),
				winrt::to_hstring(leaseId).c_str(),
				(std::min)(maxItems, 256u));

			auto http = CreateClient();
			auto requestOperation =
				http.GetAsync(Uri{ ApiUri(relative) });
			if (!WaitForCompletion(
				requestOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			auto response = requestOperation.GetResults();
			if (!response.IsSuccessStatusCode()) return std::nullopt;

			auto readOperation =
				response.Content().ReadAsStringAsync();
			if (!WaitForCompletion(
				readOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			JsonArray array = JsonArray::Parse(
				readOperation.GetResults());
			std::vector<ContentWakeup> result;
			result.reserve(array.Size());
			for (std::uint32_t i = 0; i < array.Size(); ++i)
			{
				auto json = array.GetObjectAt(i);
				ContentWakeup wakeup;
				wakeup.wakeRequestId = winrt::to_string(
					json.GetNamedString(L"wakeRequestId"));
				wakeup.contentId = winrt::to_string(
					json.GetNamedString(L"contentId"));
				wakeup.size = static_cast<std::uint64_t>(
					json.GetNamedNumber(L"size"));

				auto identities = json.GetNamedArray(L"identities");
				for (std::uint32_t index = 0; index < identities.Size(); ++index)
				{
					if (auto identity = ParseIdentity(
						identities.GetObjectAt(index)))
						wakeup.identities.push_back(std::move(*identity));
				}
				if (!wakeup.wakeRequestId.empty()
					&& !wakeup.identities.empty())
					result.push_back(std::move(wakeup));
			}
			return result;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	bool ContentDirectoryClient::CompleteWakeup(
		std::string const& nodeId,
		std::string const& leaseId,
		std::string const& wakeRequestId,
		bool succeeded,
		std::string const& canonicalInfoHashV2,
		std::vector<std::uint8_t> const& canonicalTorrent,
		std::string const& error,
		std::stop_token stopToken)
	{
		try
		{
			if (!InitializeServerDomain(stopToken))
				return false;
			JsonObject request;
			request.Insert(L"leaseId", JsonValue::CreateStringValue(
				winrt::to_hstring(leaseId)));
			request.Insert(L"succeeded", JsonValue::CreateBooleanValue(succeeded));
			request.Insert(L"canonicalProtocolVersion",
						   JsonValue::CreateNumberValue(1));

			if (succeeded)
			{
				request.Insert(L"canonicalInfoHashV2",
							   JsonValue::CreateStringValue(
								   winrt::to_hstring(canonicalInfoHashV2)));
				request.Insert(L"canonicalTorrentBase64",
							   JsonValue::CreateStringValue(
								   winrt::to_hstring(Base64(canonicalTorrent))));
			}
			else if (!error.empty())
			{
				request.Insert(L"error", JsonValue::CreateStringValue(
					winrt::to_hstring(error.substr(0, 512))));
			}

			std::wstring relative = std::format(
				L"/api/v1/content/nodes/{}/wakeups/{}/complete",
				winrt::to_hstring(nodeId).c_str(),
				winrt::to_hstring(wakeRequestId).c_str());
			auto http = CreateClient();
			HttpStringContent body{
				request.Stringify(),
				UnicodeEncoding::Utf8,
				L"application/json"
			};
			auto operation =
				http.PostAsync(Uri{ ApiUri(relative) }, body);
			if (!WaitForCompletion(
				operation,
				stopToken,
				DirectoryRequestTimeout))
				return false;
			return operation.GetResults().IsSuccessStatusCode();
		}
		catch (...)
		{
			return false;
		}
	}

	bool ContentDirectoryClient::AnnounceResource(
		std::string const& nodeId,
		std::string const& leaseId,
		ResourceKey const& resourceKey,
		ContentIdentity const& contentIdentity,
		std::stop_token stopToken)
	{
		if (nodeId.empty()
			|| leaseId.empty()
			|| !contentIdentity.IsWellFormed())
			return false;

		try
		{
			if (!InitializeServerDomain(stopToken))
				return false;

			JsonObject keyJson;
			keyJson.Insert(L"algorithm", JsonValue::CreateNumberValue(
				static_cast<int>(resourceKey.algorithm)));
			keyJson.Insert(L"digest", JsonValue::CreateStringValue(
				winrt::to_hstring(resourceKey.ToHex())));

			JsonObject request;
			request.Insert(L"leaseId", JsonValue::CreateStringValue(
				winrt::to_hstring(leaseId)));
			request.Insert(L"resourceKey", keyJson);
			request.Insert(L"contentIdentity", IdentityJson(contentIdentity));

			std::wstring relative = L"/api/v1/content/nodes/";
			relative += winrt::to_hstring(nodeId).c_str();
			relative += L"/resources";

			auto http = CreateClient();
			HttpStringContent body{
				request.Stringify(),
				UnicodeEncoding::Utf8,
				L"application/json"
			};
			auto operation = http.PostAsync(
				Uri{ ApiUri(relative) }, body);
			if (!WaitForCompletion(
				operation,
				stopToken,
				DirectoryRequestTimeout))
				return false;
			return operation.GetResults().IsSuccessStatusCode();
		}
		catch (...)
		{
			return false;
		}
	}

	std::optional<ResourceLookupResult>
		ContentDirectoryClient::LookupResource(
			ResourceKey const& resourceKey,
			std::uint32_t maxCandidates,
			std::stop_token stopToken)
	{
		try
		{
			if (!InitializeServerDomain(stopToken))
				return std::nullopt;

			std::wstring relative = std::format(
				L"/api/v1/content/resources/lookup"
				L"?algorithm={}&digest={}&maxCandidates={}",
				static_cast<int>(resourceKey.algorithm),
				winrt::to_hstring(resourceKey.ToHex()).c_str(),
				(std::min)(maxCandidates, 32u));

			auto http = CreateClient();
			auto requestOperation = http.GetAsync(
				Uri{ ApiUri(relative) });
			if (!WaitForCompletion(
				requestOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			auto response = requestOperation.GetResults();
			if (!response.IsSuccessStatusCode())
				return std::nullopt;

			auto readOperation =
				response.Content().ReadAsStringAsync();
			if (!WaitForCompletion(
				readOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			JsonObject json = JsonObject::Parse(
				readOperation.GetResults());

			ResourceLookupResult result;
			result.key = resourceKey;

			auto candidates = json.GetNamedArray(L"candidates");
			for (std::uint32_t i = 0; i < candidates.Size(); ++i)
			{
				auto candidateJson = candidates.GetObjectAt(i);
				ResourceCandidate candidate;
				candidate.contentId = winrt::to_string(
					candidateJson.GetNamedString(L"contentId"));
				candidate.size = static_cast<std::uint64_t>(
					candidateJson.GetNamedNumber(L"size"));
				candidate.observationCount =
					static_cast<std::uint32_t>(
						candidateJson.GetNamedNumber(
							L"observationCount", 0));

				auto identities =
					candidateJson.GetNamedArray(L"identities");
				for (std::uint32_t index = 0;
					 index < identities.Size();
					 ++index)
				{
					if (auto identity = ParseIdentity(
						identities.GetObjectAt(index)))
						candidate.identities.push_back(
							std::move(*identity));
				}

				if (!candidate.contentId.empty()
					&& !candidate.identities.empty())
					result.candidates.push_back(
						std::move(candidate));
			}

			return result.candidates.empty()
				? std::nullopt
				: std::optional<ResourceLookupResult>{
					std::move(result) };
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::optional<ContentLookupResult> ContentDirectoryClient::Lookup(
		ContentIdentity const& identity,
		std::optional<std::string> const& excludeNodeId,
		std::uint32_t maxPeers,
		bool prepare,
		std::stop_token stopToken)
	{
		if (!identity.IsWellFormed()) return std::nullopt;

		try
		{
			if (!InitializeServerDomain(stopToken))
				return std::nullopt;
			std::wstring relative = std::format(
				L"/api/v1/content/lookup?algorithm={}&digest={}&maxPeers={}&prepare={}",
				static_cast<int>(identity.algorithm),
				winrt::to_hstring(identity.ToHex()).c_str(),
				(std::min)(maxPeers, 100u),
				prepare ? L"true" : L"false");
			if (excludeNodeId && !excludeNodeId->empty())
			{
				relative += L"&excludeNodeId=";
				relative += winrt::to_hstring(*excludeNodeId).c_str();
			}

			auto http = CreateClient();
			auto requestOperation =
				http.GetAsync(Uri{ ApiUri(relative) });
			if (!WaitForCompletion(
				requestOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			auto response = requestOperation.GetResults();
			if (!response.IsSuccessStatusCode()) return std::nullopt;

			auto readOperation =
				response.Content().ReadAsStringAsync();
			if (!WaitForCompletion(
				readOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			JsonObject json = JsonObject::Parse(
				readOperation.GetResults());
			ContentLookupResult result;
			result.contentId = winrt::to_string(
				json.GetNamedString(L"contentId"));
			result.identity = identity;
			result.size = static_cast<std::uint64_t>(
				json.GetNamedNumber(L"size"));
			result.canonicalProtocolVersion = static_cast<std::uint32_t>(
				json.GetNamedNumber(L"canonicalProtocolVersion", 0));
			result.canonicalInfoHashV2 = winrt::to_string(
				json.GetNamedString(L"canonicalInfoHashV2", L""));
			result.manifestAvailable =
				json.GetNamedBoolean(L"manifestAvailable", false);
			result.retryAfterMilliseconds = static_cast<std::uint32_t>(
				json.GetNamedNumber(L"retryAfterMilliseconds", 1000));

			auto peers = json.GetNamedArray(L"peers");
			for (std::uint32_t peerIndex = 0;
				 peerIndex < peers.Size(); ++peerIndex)
			{
				auto peerJson = peers.GetObjectAt(peerIndex);
				ContentPeer peer;
				peer.nodeId = winrt::to_string(
					peerJson.GetNamedString(L"nodeId"));
				peer.ready = peerJson.GetNamedBoolean(L"ready", false);

				auto endpoints = peerJson.GetNamedArray(L"endpoints");
				for (std::uint32_t endpointIndex = 0;
					 endpointIndex < endpoints.Size(); ++endpointIndex)
				{
					auto endpointJson = endpoints.GetObjectAt(endpointIndex);
					ContentPeerEndpoint endpoint;
					endpoint.address = winrt::to_string(
						endpointJson.GetNamedString(L"address"));
					endpoint.isIpv6 =
						endpointJson.GetNamedBoolean(L"isIpv6");
					endpoint.port = static_cast<std::uint16_t>(
						endpointJson.GetNamedNumber(L"port"));
					endpoint.transport = ParseTransport(
						endpointJson.GetNamedString(L"transport"));
					endpoint.verification = winrt::to_string(
						endpointJson.GetNamedString(
							L"verification", L""));
					peer.endpoints.push_back(std::move(endpoint));
				}
				result.peers.push_back(std::move(peer));
			}
			return result;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::optional<std::vector<std::uint8_t>>
		ContentDirectoryClient::GetManifest(
			std::string const& contentId,
			std::stop_token stopToken)
	{
		if (contentId.empty()) return std::nullopt;
		try
		{
			if (!InitializeServerDomain(stopToken))
				return std::nullopt;
			std::wstring relative = L"/api/v1/content/manifests/";
			relative += winrt::to_hstring(contentId).c_str();

			auto http = CreateClient();
			auto requestOperation =
				http.GetAsync(Uri{ ApiUri(relative) });
			if (!WaitForCompletion(
				requestOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			auto response = requestOperation.GetResults();
			if (!response.IsSuccessStatusCode()) return std::nullopt;

			auto readOperation =
				response.Content().ReadAsBufferAsync();
			if (!WaitForCompletion(
				readOperation,
				stopToken,
				DirectoryRequestTimeout))
				return std::nullopt;
			auto buffer = readOperation.GetResults();
			DataReader reader = DataReader::FromBuffer(buffer);
			std::vector<std::uint8_t> bytes(buffer.Length());
			if (!bytes.empty())
				reader.ReadBytes(winrt::array_view<std::uint8_t>(bytes));
			return bytes;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}
}
