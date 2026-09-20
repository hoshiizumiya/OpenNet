export module OpenNet.Web.ServerDomain;

export import OpenNet.Web.ServerDomainMode;

import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Core.Setting.LocalSetting;
import winrt.Windows.Data.Json;
import winrt.Windows.Foundation;
import winrt.Windows.Web.Http;
import std;

namespace OpenNet::Web::ServerDomain::details
{
	using namespace winrt;
	using namespace winrt::Windows::Data::Json;
	using namespace winrt::Windows::Foundation;
	using namespace winrt::Windows::Web::Http;

	inline constexpr wchar_t ConfigurationUri[] =
		L"https://raw.gitcode.com/hoshiizumiya/OpenNet.Server.Domain/raw/main/domain.json";
	inline constexpr wchar_t DefaultDomainRoot[] = L"http://opennet.hoshiizumiya.top:5090/";
	inline constexpr wchar_t DefaultIpRoot[] = L"http://103.236.69.23:5090/";
	inline constexpr std::uint32_t ConfigurationTimeoutMs = 2000;
	inline constexpr std::uint32_t EndpointProbeTimeoutMs = 1000;

	std::atomic<ServerDomainMode> CurrentMode{ ServerDomainMode::Primary };
	std::shared_mutex EndpointMutex;
	std::wstring PrimaryRoot{ DefaultDomainRoot };
	std::wstring BackupRoot{ DefaultIpRoot };

	[[nodiscard]]
	bool IsBackup() noexcept
	{
		return CurrentMode.load(std::memory_order_relaxed) == ServerDomainMode::Backup;
	}

	[[nodiscard]]
	std::wstring CurrentRoot()
	{
		std::shared_lock lock{ EndpointMutex };
		return IsBackup() ? BackupRoot : PrimaryRoot;
	}

	template<typename TAsync>
	IAsyncOperation<bool> WaitForCompletionAsync(TAsync const& operation, std::uint32_t timeoutMs)
	{
		auto cancellation = co_await winrt::get_cancellation_token();
		cancellation.enable_propagation();
		auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

		for (;;)
		{
			if (cancellation())
			{
				operation.Cancel();
				co_return false;
			}

			auto const status = operation.Status();
			if (status == AsyncStatus::Completed)
				co_return true;
			if (status != AsyncStatus::Started)
				co_return false;
			if (std::chrono::steady_clock::now() >= deadline)
			{
				operation.Cancel();
				co_return false;
			}

			co_await winrt::resume_after(std::chrono::milliseconds(50));
		}
	}

	[[nodiscard]]
	std::wstring NormalizeType(winrt::hstring const& value)
	{
		std::wstring normalized{ value.c_str(), value.size() };
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t character)
		{
			return static_cast<wchar_t>(std::towlower(character));
		});
		return normalized;
	}

	[[nodiscard]]
	std::optional<std::wstring> BuildRoot(JsonObject const& endpoint)
	{
		auto const nameValue = endpoint.GetNamedString(L"name", L"");
		auto const protocolValue = endpoint.GetNamedString(L"protocol", L"http");
		if (nameValue.empty() || protocolValue.empty())
			return std::nullopt;

		std::wstring protocol{ protocolValue.c_str(), protocolValue.size() };
		std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](wchar_t character)
		{
			return static_cast<wchar_t>(std::towlower(character));
		});
		if (protocol != L"http" && protocol != L"https")
			return std::nullopt;

		std::wstring host{ nameValue.c_str(), nameValue.size() };
		if (host.find(L':') != std::wstring::npos && !host.starts_with(L'['))
		{
			host.insert(host.begin(), L'[');
			host.push_back(L']');
		}

		auto const rawPort = endpoint.GetNamedNumber(L"port", 0.0);
		if (rawPort < 0.0 || rawPort > 65535.0)
			return std::nullopt;
		auto const port = static_cast<std::uint16_t>(rawPort);

		std::wstring root = protocol;
		root.append(L"://");
		root.append(host);
		if (port != 0)
		{
			root.push_back(L':');
			root.append(std::to_wstring(port));
		}
		root.push_back(L'/');

		try
		{
			(void)Uri{ root };
		}
		catch (...)
		{
			return std::nullopt;
		}
		return root;
	}

	bool TryParseConfiguration(
		winrt::hstring const& json,
		std::vector<std::wstring>& domains,
		std::vector<std::wstring>& ips)
	{
		JsonObject root;
		if (!JsonObject::TryParse(json, root))
			return false;

		auto const data = root.GetNamedArray(L"data", JsonArray{});
		std::vector<std::wstring> parsedDomains;
		std::vector<std::wstring> parsedIps;

		for (auto const& item : data)
		{
			if (item.ValueType() != JsonValueType::Object)
				continue;

			auto const endpoint = item.GetObject();
			auto const type = NormalizeType(endpoint.GetNamedString(L"type", L""));
			auto const candidate = BuildRoot(endpoint);
			if (!candidate)
				continue;

			if (type == L"domain")
				parsedDomains.emplace_back(*candidate);
			else if (type == L"ip")
				parsedIps.emplace_back(*candidate);
		}

		if (parsedDomains.empty() && parsedIps.empty())
			return false;

		if (!parsedDomains.empty())
			domains = std::move(parsedDomains);
		if (!parsedIps.empty())
			ips = std::move(parsedIps);
		return true;
	}

	IAsyncOperation<bool> ProbeEndpointAsync(std::wstring const& root)
	{
		try
		{
			HttpClient client;
			client.DefaultRequestHeaders().UserAgent().ParseAdd(L"OpenNet/1.0 ServerDomainProbe");
			auto operation = client.GetAsync(Uri{ root }, HttpCompletionOption::ResponseHeadersRead);
			if (!co_await WaitForCompletionAsync(operation, EndpointProbeTimeoutMs))
				co_return false;

			(void)operation.GetResults();
			co_return true;
		}
		catch (...)
		{
			co_return false;
		}
	}

	void SetResolvedRoots(std::wstring primary, std::wstring backup)
	{
		std::unique_lock lock{ EndpointMutex };
		PrimaryRoot = std::move(primary);
		BackupRoot = std::move(backup);
	}
}

export namespace OpenNet::Web::ServerDomain
{
	void SetMode(ServerDomainMode mode) noexcept
	{
		details::CurrentMode.store(mode, std::memory_order_relaxed);
	}

	winrt::Windows::Foundation::IAsyncAction InitializeAsync()
	{
		using namespace winrt;
		using namespace winrt::Windows::Foundation;
		using namespace winrt::Windows::Web::Http;

		std::vector<std::wstring> domainCandidates{ details::DefaultDomainRoot };
		std::vector<std::wstring> ipCandidates{ details::DefaultIpRoot };

		try
		{
			HttpClient client;
			client.DefaultRequestHeaders().UserAgent().ParseAdd(L"OpenNet/1.0 ServerDomainResolver");
			auto operation = client.GetStringAsync(Uri{ details::ConfigurationUri });
			if (co_await details::WaitForCompletionAsync(operation, details::ConfigurationTimeoutMs))
			{
				auto const json = operation.GetResults();
				(void)details::TryParseConfiguration(json, domainCandidates, ipCandidates);
			}
		}
		catch (...)
		{
			// Keep the compiled defaults when the remote configuration is unavailable.
		}

		details::SetResolvedRoots(domainCandidates.front(), ipCandidates.front());

		// Domain entries always have priority. Only after every domain candidate
		// fails do we try IP entries.
		for (auto const& candidate : domainCandidates)
		{
			if (co_await details::ProbeEndpointAsync(candidate))
			{
				details::SetResolvedRoots(candidate, ipCandidates.front());
				details::CurrentMode.store(ServerDomainMode::Primary, std::memory_order_relaxed);
				co_return;
			}
		}

		for (auto const& candidate : ipCandidates)
		{
			if (co_await details::ProbeEndpointAsync(candidate))
			{
				details::SetResolvedRoots(domainCandidates.front(), candidate);
				details::CurrentMode.store(ServerDomainMode::Backup, std::memory_order_relaxed);
				co_return;
			}
		}

		// If the machine is currently offline, retain the domain as the preferred
		// endpoint so a later network recovery still follows the intended policy.
		details::CurrentMode.store(ServerDomainMode::Primary, std::memory_order_relaxed);
	}

	[[nodiscard]]
	std::wstring GetHomeRoot()
	{
		return details::CurrentRoot();
	}

	[[nodiscard]]
	std::wstring GetApiRoot()
	{
		return details::CurrentRoot();
	}

	[[nodiscard]]
	std::wstring GetRootDomain()
	{
		return details::CurrentRoot();
	}

	void TryAutoFallback()
	{
		ServerDomainMode expected = ServerDomainMode::Primary;
		if (details::CurrentMode.compare_exchange_strong(
			expected,
			ServerDomainMode::Backup,
			std::memory_order_relaxed))
		{
			OpenNet::Core::Setting::LocalSetting::Set(
				OpenNet::Core::Setting::SettingKeys::ServerDomainMode,
				ServerDomainMode::Backup);
		}
	}

	[[nodiscard]]
	bool IsBackupMode() noexcept
	{
		return details::IsBackup();
	}
}
