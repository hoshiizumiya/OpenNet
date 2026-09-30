export module OpenNet.Web.ServerDomain;

export import OpenNet.Web.ServerDomainMode;

import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Core.Setting.LocalSetting;
import winrt.Windows.Data.Json;
import winrt.Windows.Foundation;
import winrt.Windows.Web.Http;
import std;

export namespace OpenNet::Web::ServerDomain
{
	struct EndpointInfo
	{
		std::wstring Root;
		ServerDomainMode Mode;
		bool Available;
	};

	struct EndpointStatus
	{
		std::vector<EndpointInfo> Endpoints;
		std::wstring CurrentRoot;
		ServerDomainMode PreferredMode{ ServerDomainMode::AutoDetect };
		bool ConfigurationLoaded{ false };
	};
}

namespace OpenNet::Web::ServerDomain::details
{
	using namespace winrt;
	using namespace winrt::Windows::Data::Json;
	using namespace winrt::Windows::Foundation;
	using namespace winrt::Windows::Web::Http;

	inline constexpr wchar_t ConfigurationUri[] = L"https://raw.gitcode.com/hoshiizumiya/OpenNet.Server.Domain/raw/main/domain.json";
	inline constexpr std::uint32_t ConfigurationTimeoutMs = 2000;
	inline constexpr std::uint32_t EndpointProbeTimeoutMs = 1000;

	enum class InitializationState : std::uint8_t
	{
		NotStarted,
		Running,
		Completed
	};
	std::atomic<InitializationState> Initialization{ InitializationState::NotStarted };
	std::shared_mutex EndpointMutex;
	EndpointStatus Status;
	bool ModeChangedByUser{ false };

	[[nodiscard]] bool IsValidMode(ServerDomainMode mode) noexcept
	{
		return mode == ServerDomainMode::AutoDetect || mode == ServerDomainMode::Ip
			|| mode == ServerDomainMode::Http || mode == ServerDomainMode::Https;
	}

	void SelectEndpoint()
	{
		// EndpointMutex is held by the caller. A manual choice only uses that
		// category; automatic mode tries HTTPS, HTTP domains, then IP addresses.
		auto select = [](std::vector<EndpointInfo> const& endpoints, ServerDomainMode mode)
			-> std::wstring
		{
			for (auto const& endpoint : endpoints)
				if (endpoint.Available && endpoint.Mode == mode) return endpoint.Root;
			return {};
		};
		if (Status.PreferredMode != ServerDomainMode::AutoDetect)
		{
			Status.CurrentRoot = select(Status.Endpoints, Status.PreferredMode);
			return;
		}
		Status.CurrentRoot.clear();
		for (auto const mode : { ServerDomainMode::Https,
			 ServerDomainMode::Http, ServerDomainMode::Ip })
		{
			Status.CurrentRoot = select(Status.Endpoints, mode);
			if (!Status.CurrentRoot.empty()) break;
		}
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
				operation.Cancel(); co_return false;
			}
			auto const status = operation.Status();
			if (status == AsyncStatus::Completed) co_return true;
			if (status != AsyncStatus::Started) co_return false;
			if (std::chrono::steady_clock::now() >= deadline)
			{
				operation.Cancel();
				co_return false;
			}
			co_await winrt::resume_after(std::chrono::milliseconds(50));
		}
	}

	[[nodiscard]] std::wstring Lowercase(winrt::hstring const& value)
	{
		std::wstring result{ value.c_str(), value.size() };
		std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch)
		{
			return static_cast<wchar_t>(std::towlower(ch));
		});
		return result;
	}

	[[nodiscard]] std::optional<EndpointInfo> ParseEndpoint(JsonObject const& endpoint)
	{
		auto const type = Lowercase(endpoint.GetNamedString(L"type", L""));
		auto const protocol = Lowercase(endpoint.GetNamedString(L"protocol", L""));
		if ((type != L"domain" && type != L"ip")
			|| (protocol != L"http" && protocol != L"https")) return std::nullopt;

		auto const name = endpoint.GetNamedString(L"name", L"");
		if (name.empty()) return std::nullopt;
		std::wstring host{ name.c_str(), name.size() };
		if (host.find_first_of(L"/\\?#@ \t\r\n") != std::wstring::npos)
			return std::nullopt;
		if (host.find(L':') != std::wstring::npos && !host.starts_with(L'['))
		{
			host.insert(host.begin(), L'[');
			host.push_back(L']');
		}
		auto const rawPort = endpoint.GetNamedNumber(L"port", 0.0);
		if (!std::isfinite(rawPort) || rawPort < 0.0 || rawPort > 65535.0
			|| std::floor(rawPort) != rawPort) return std::nullopt;
		auto const port = static_cast<std::uint16_t>(rawPort);
		std::wstring root = protocol + L"://" + host;
		if (port != 0) root += L":" + std::to_wstring(port);
		root.push_back(L'/');
		try
		{
			Uri uri{ root };
			if (Lowercase(uri.SchemeName()) != protocol || uri.Host().empty())
				return std::nullopt;
		}
		catch (...)
		{
			return std::nullopt;
		}
		return EndpointInfo{ std::move(root), type == L"ip" ? ServerDomainMode::Ip
			: protocol == L"https" ? ServerDomainMode::Https : ServerDomainMode::Http, false };
	}

	[[nodiscard]] std::optional<std::vector<EndpointInfo>> ParseConfiguration(winrt::hstring const& json)
	{
		JsonObject root;
		if (!JsonObject::TryParse(json, root) || !root.HasKey(L"data")
			|| root.GetNamedValue(L"data").ValueType() != JsonValueType::Array)
			return std::nullopt;
		std::vector<EndpointInfo> endpoints;
		for (auto const& item : root.GetNamedArray(L"data", JsonArray{}))
		{
			if (item.ValueType() != JsonValueType::Object) continue;
			try
			{
				if (auto endpoint = ParseEndpoint(item.GetObject()))
				{
					auto const duplicate = std::ranges::find_if(endpoints, [&](auto const& existing)
					{
						return existing.Root == endpoint->Root;
					});
					if (duplicate == endpoints.end()) endpoints.push_back(std::move(*endpoint));
				}
			}
			catch (...)
			{ /* Ignore one malformed entry. */
			}
		}
		return endpoints;
	}

	IAsyncOperation<bool> ProbeEndpointAsync(std::wstring const& root)
	{
		try
		{
			HttpClient client;
			client.DefaultRequestHeaders().UserAgent().ParseAdd(L"OpenNet/1.0 ServerDomainProbe");
			auto operation = client.GetAsync(Uri{ root }, HttpCompletionOption::ResponseHeadersRead);
			if (!co_await WaitForCompletionAsync(operation, EndpointProbeTimeoutMs)) co_return false;
			(void)operation.GetResults();
			co_return true;
		}
		catch (...)
		{
			co_return false;
		}
	}
}

export namespace OpenNet::Web::ServerDomain
{
	void SetMode(ServerDomainMode mode)
	{
		if (!details::IsValidMode(mode)) mode = ServerDomainMode::AutoDetect;
		{
			std::unique_lock lock{ details::EndpointMutex };
			details::Status.PreferredMode = mode;
			details::ModeChangedByUser = true;
			details::SelectEndpoint();
		}
		OpenNet::Core::Setting::LocalSetting::Set(
			OpenNet::Core::Setting::SettingKeys::ServerDomainMode, mode);
	}

	[[nodiscard]] EndpointStatus GetStatus()
	{
		std::shared_lock lock{ details::EndpointMutex };
		return details::Status;
	}

	winrt::Windows::Foundation::IAsyncAction InitializeAsync()
	{
		using namespace winrt;
		using namespace winrt::Windows::Foundation;
		using namespace winrt::Windows::Web::Http;
		if (details::Initialization.load(std::memory_order_acquire)
			== details::InitializationState::Completed) co_return;
		auto expected = details::InitializationState::NotStarted;
		if (!details::Initialization.compare_exchange_strong(expected,
															 details::InitializationState::Running, std::memory_order_acq_rel))
		{
			while (details::Initialization.load(std::memory_order_acquire)
				   == details::InitializationState::Running)
				co_await winrt::resume_after(std::chrono::milliseconds(25));
			co_return;
		}
		try
		{
			auto mode = ServerDomainMode::AutoDetect;
			try
			{
				mode = OpenNet::Core::Setting::LocalSetting::Get<ServerDomainMode>(
					OpenNet::Core::Setting::SettingKeys::ServerDomainMode,
					ServerDomainMode::AutoDetect);
			}
			catch (...)
			{
			}
			if (!details::IsValidMode(mode)) mode = ServerDomainMode::AutoDetect;
			{
				std::unique_lock lock{ details::EndpointMutex };
				if (!details::ModeChangedByUser)
					details::Status.PreferredMode = mode;
			}
			HttpClient client;
			client.DefaultRequestHeaders().UserAgent().ParseAdd(L"OpenNet/1.0 ServerDomainResolver");
			auto operation = client.GetStringAsync(Uri{ details::ConfigurationUri });
			if (co_await details::WaitForCompletionAsync(operation, details::ConfigurationTimeoutMs))
			{
				if (auto endpoints = details::ParseConfiguration(operation.GetResults()))
				{
					for (auto& endpoint : *endpoints)
						endpoint.Available = co_await details::ProbeEndpointAsync(endpoint.Root);
					std::unique_lock lock{ details::EndpointMutex };
					details::Status.Endpoints = std::move(*endpoints);
					details::Status.ConfigurationLoaded = true;
					details::SelectEndpoint();
				}
			}
		}
		catch (...)
		{ /* No compiled endpoint substitutes for the directory. */
		}
		details::Initialization.store(details::InitializationState::Completed,
									  std::memory_order_release);
	}

	winrt::Windows::Foundation::IAsyncAction RefreshAsync()
	{
		while (details::Initialization.load(std::memory_order_acquire)
			   == details::InitializationState::Running)
			co_await winrt::resume_after(std::chrono::milliseconds(25));
		auto expected = details::InitializationState::Completed;
		details::Initialization.compare_exchange_strong(expected,
														details::InitializationState::NotStarted, std::memory_order_acq_rel);
		co_await InitializeAsync();
	}

	[[nodiscard]] std::wstring GetHomeRoot()
	{
		std::shared_lock lock{ details::EndpointMutex };
		return details::Status.CurrentRoot;
	}
	[[nodiscard]] std::wstring GetApiRoot()
	{
		std::shared_lock lock{ details::EndpointMutex };
		return details::Status.CurrentRoot;
	}
	[[nodiscard]] std::wstring GetRootDomain()
	{
		return GetApiRoot();
	}

}
