export module OpenNet.Core.P2PManager;

import std;
import OpenNet.Core.torrentCore.LibtorrentHandle;
import OpenNet.Core.torrentCore.TorrentStateManager;
import winrt.Windows.Foundation;

export namespace OpenNet::Core
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
		::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats
			GetSessionStats();
		::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats
			GetPerformanceStats();
		std::vector<::OpenNet::Core::Torrent::LibtorrentHandle::TorrentPeerInfo>
			GetTorrentPeers(std::string const& taskId);

		// State manager access
		::OpenNet::Core::Torrent::TorrentStateManager* StateManager() noexcept
		{
			return m_stateManager.get();
		}

		// 关闭和清理资源
		void Shutdown();

		// Torrent operations
		winrt::Windows::Foundation::IAsyncOperation<bool> AddMagnetAsync(
			std::string magnetUri,
			std::string savePath,
			std::vector<int> const& filePriorities = {},
			std::vector<std::string> const& extraTrackers = {},
			bool startImmediately = true);
		winrt::Windows::Foundation::IAsyncOperation<bool> AddTorrentFileAsync(
			std::string torrentFilePath,
			std::string savePath,
			std::vector<int> const& filePriorities = {},
			std::vector<std::string> const& extraTrackers = {},
			bool startImmediately = true);

		// Load all saved tasks and resume them
		winrt::Windows::Foundation::IAsyncAction LoadAndResumeSavedTasksAsync();

		// Get all saved task metadata
		std::vector<::OpenNet::Core::Torrent::TaskMetadata> GetAllTasks();

		// Import/Export task data
		winrt::Windows::Foundation::IAsyncOperation<bool> ExportTasksAsync(std::wstring filePath);
		winrt::Windows::Foundation::IAsyncOperation<bool> ImportTasksAsync(std::wstring filePath);

		// Callback registration
		using ProgressCb = std::function<void(const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent&)>;
		using FinishedCb = std::function<void(const std::string&, const std::string&)>;
		using ErrorCb = std::function<void(const std::string&)>;

		void SetProgressCallback(ProgressCb cb);
		void SetFinishedCallback(FinishedCb cb);
		void SetErrorCallback(ErrorCb cb);

	private:
		enum class InitializationState
		{
			Uninitialized,
			Initializing,
			Initialized,
			ShuttingDown,
		};

		P2PManager() = default;
		~P2PManager() = default;

		void WireCoreCallbacks();
		winrt::fire_and_forget ProbePortAfterStartupAsync();

		std::unique_ptr<::OpenNet::Core::Torrent::LibtorrentHandle> m_torrentCore;
		std::unique_ptr<::OpenNet::Core::Torrent::TorrentStateManager> m_stateManager;
		std::mutex m_torrentMutex;
		// Concurrent callers share one attempt and receive its success or
		// failure. This avoids polling forever when initialization fails.
		std::mutex m_lifecycleMutex;
		InitializationState m_initializationState{ InitializationState::Uninitialized };
		std::shared_future<void> m_initializationCompletion;
		std::atomic<bool> m_isTorrentCoreInitialized{ false };

		std::mutex m_cbMutex;
		ProgressCb m_progressCb;
		FinishedCb m_finishedCb;
		ErrorCb m_errorCb;
	};
}
