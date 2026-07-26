export module OpenNet.Web.ServerDomain;

export import OpenNet.Web.ServerDomainMode;

import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Core.Setting.LocalSetting;
import std;

namespace OpenNet::Web::ServerDomain::details
{
	// 模块内部状态，不会导出给模块使用者
	std::atomic<ServerDomainMode> CurrentMode
	{
		ServerDomainMode::Primary
	};

	[[nodiscard]]
	bool IsBackup() noexcept
	{
		return CurrentMode.load(std::memory_order_relaxed)
			== ServerDomainMode::Backup;
	}
}

export namespace OpenNet::Web::ServerDomain
{
	void SetMode(ServerDomainMode mode) noexcept
	{
		details::CurrentMode.store(
			mode,
			std::memory_order_relaxed);
	}

	[[nodiscard]]
	std::wstring_view GetHomeRoot() noexcept
	{
		return details::IsBackup()
			? L"http://103.236.69.23:5090/"
			: L"http://103.236.69.23:5090/";
	}

	[[nodiscard]]
	std::wstring_view GetApiRoot() noexcept
	{
		return details::IsBackup()
			? L"http://103.236.69.23:5090/"
			: L"http://103.236.69.23:5090/";
	}

	[[nodiscard]]
	std::wstring_view GetRootDomain() noexcept
	{
		return details::IsBackup()
			? L"http://103.236.69.23:5090/"
			: L"http://103.236.69.23:5090/";
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
