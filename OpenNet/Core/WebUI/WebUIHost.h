#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace OpenNet::Core::WebUI
{
	struct WebUIOptions
	{
		std::string address{ "127.0.0.1" };
		std::uint16_t port{ 8080 };
		std::string username{ "admin" };
		std::string password{ "adminadmin" };
		std::string locale{ "en" };
		// "qbittorrent" or "vuetorrent". Both use the same OpenNet Web API.
		std::string frontend{ "qbittorrent" };
		std::filesystem::path assetRoot;
		std::size_t workerThreads{ 2 };
		std::function<void()> shutdownCallback;
	};

	class WebUIHost final
	{
	public:
		static WebUIHost& Instance();

		WebUIHost(WebUIHost const&) = delete;
		WebUIHost& operator=(WebUIHost const&) = delete;

		bool Start(WebUIOptions options = {});
		bool Restart();
		void Stop() noexcept;

		[[nodiscard]] bool IsRunning() const noexcept;
		[[nodiscard]] std::uint16_t Port() const noexcept;
		[[nodiscard]] std::filesystem::path AssetRoot() const;

	private:
		class Impl;

		WebUIHost();
		~WebUIHost();

		std::unique_ptr<Impl> m_impl;
	};
}
