#pragma once

#include <winrt/Windows.Foundation.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <string>

// Include torrent core so nested ProgressEvent is known
#include "Core/torrentCore/libtorrentHandle.h"

namespace OpenNet::Core
{
	// 单例设计模式的 P2PManager 类
	class P2PManager
	{
	public:
		// Set and then Get the libtorrent Core
		static P2PManager& Instance();
		// 删除拷贝构造函数与拷贝赋值运算符。
		// 阻止外部复制单例对象，防止出现多个实例。
		// = delete 是 C++11 引入的语法，用于显式禁用函数。
		P2PManager(P2PManager const&) = delete;
		P2PManager& operator=(P2PManager const&) = delete;

		// Core lifecycle
		winrt::Windows::Foundation::IAsyncAction EnsureTorrentCoreInitializedAsync();
		bool IsTorrentCoreInitialized() const noexcept
		{
			return m_isTorrentCoreInitialized.load();
		}
		::OpenNet::Core::Torrent::LibtorrentHandle* TorrentCore() noexcept
		{
			return m_torrentCore.get();
		}

		// Torrent operations
		winrt::Windows::Foundation::IAsyncOperation<bool> AddMagnetAsync(std::string magnetUri, std::string savePath);

		// Callback registration
		using ProgressCb = std::function<void(const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent&)>;
		using FinishedCb = std::function<void(const std::string&)>;
		using ErrorCb = std::function<void(const std::string&)>;

		void SetProgressCallback(ProgressCb cb);
		void SetFinishedCallback(FinishedCb cb);
		void SetErrorCallback(ErrorCb cb);

	private:
		P2PManager() = default;
		~P2PManager() = default;

		void WireCoreCallbacks();

		std::unique_ptr<::OpenNet::Core::Torrent::LibtorrentHandle> m_torrentCore;
		std::mutex m_torrentMutex;
		std::atomic<bool> m_isTorrentCoreInitialized{ false };
		std::atomic<bool> m_initializing{ false };

		std::mutex m_cbMutex;
		ProgressCb m_progressCb;
		FinishedCb m_finishedCb;
		ErrorCb m_errorCb;
	};
}