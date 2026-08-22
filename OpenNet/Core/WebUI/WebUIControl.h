#pragma once

#include <cstdint>

namespace OpenNet::Core::WebUI
{
	struct WebUIRuntimeStats
	{
		std::uint64_t requests{};
		std::uint64_t requestBytes{};
		std::uint64_t responseBytes{};
		std::uint64_t activeConnections{};
		std::uint64_t failedLogins{};
	};

	bool IsWebUIRunning() noexcept;
	bool StartWebUI();
	bool RestartWebUI();
	void StopWebUI() noexcept;
	WebUIRuntimeStats GetWebUIRuntimeStats() noexcept;
}
