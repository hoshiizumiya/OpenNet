#pragma once

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

namespace OpenNet::Core
{
    // 传输状态 / Transfer Status
    enum class TransferStatus
    {
        Pending,                       // 等待中 / Pending
        Initializing,                  // 初始化 / Initializing
        Transferring,                  // 传输中 / Transferring
        Paused,                        // 已暂停 / Paused
        Completed,                     // 已完成 / Completed
        Failed,                        // 失败 / Failed
        Cancelled,                     // 已取消 / Cancelled
        Verifying                      // 校验中 / Verifying
    };

    // 传输方向 / Transfer Direction
    enum class TransferDirection
    {
        Upload,                        // 上传 / Upload
        Download                       // 下载 / Download
    };

    // 文件分块信息 / File Chunk Information
    struct FileChunk
    {
        uint32_t chunkIndex;           // 分块索引 / Chunk Index
        uint64_t offset;               // 文件偏移 / File Offset
        uint32_t size;                 // 分块大小 / Chunk Size
        std::vector<uint8_t> hash;     // 分块哈希 / Chunk Hash
        bool isComplete;               // 是否完成 / Is Complete
        bool isVerified;               // 是否校验 / Is Verified
        std::chrono::system_clock::time_point downloadTime; // 下载时间 / Download Time

        FileChunk()
            : chunkIndex(0)
            , offset(0)
            , size(0)
            , isComplete(false)
            , isVerified(false)
        {
        }
    };

    // 传输会话信息 / Transfer Session Information
    struct TransferSession
    {
        std::wstring transferId;       // 传输ID / Transfer ID
        std::wstring sessionId;        // 会话ID / Session ID
        std::wstring peerId;           // 对等节点ID / Peer ID
        std::wstring fileName;         // 文件名 / File Name
        std::wstring filePath;         // 文件路径 / File Path
        uint64_t fileSize;             // 文件大小 / File Size
        std::vector<uint8_t> fileHash; // 文件哈希 / File Hash
        TransferDirection direction;   // 传输方向 / Transfer Direction
        TransferStatus status;         // 传输状态 / Transfer Status

        // 分块信息 / Chunk Information
        std::vector<FileChunk> chunks; // 分块列表 / Chunk List
        uint32_t chunkSize;            // 分块大小 / Chunk Size
        uint32_t totalChunks;          // 总分块数 / Total Chunks
        uint32_t completedChunks;      // 已完成分块数 / Completed Chunks

        // 传输统计 / Transfer Statistics
        uint64_t transferredBytes;     // 已传输字节 / Transferred Bytes
        double transferSpeed;          // 传输速度 / Transfer Speed
        std::chrono::system_clock::time_point startTime;    // 开始时间 / Start Time
        std::chrono::system_clock::time_point endTime;      // 结束时间 / End Time
        std::chrono::milliseconds estimatedTimeRemaining;   // 预计剩余时间 / ETA

        // 传输选项 / Transfer Options
        bool enableCompression;        // 启用压缩 / Enable Compression
        bool enableEncryption;         // 启用加密 / Enable Encryption
        bool enableResume;             // 启用断点续传 / Enable Resume
        bool verifyIntegrity;          // 校验完整性 / Verify Integrity
        uint32_t maxConcurrentChunks;  // 最大并发分块数 / Max Concurrent Chunks
        uint64_t bandwidthLimit;       // 带宽限制 / Bandwidth Limit

        // 错误信息 / Error Information
        std::wstring lastError;        // 最后错误 / Last Error
        uint32_t retryCount;           // 重试次数 / Retry Count

        TransferSession()
            : fileSize(0)
            , direction(TransferDirection::Download)
            , status(TransferStatus::Pending)
            , chunkSize(1024 * 1024) // 1MB默认 / 1MB default
            , totalChunks(0)
            , completedChunks(0)
            , transferredBytes(0)
            , transferSpeed(0.0)
            , estimatedTimeRemaining(std::chrono::milliseconds::zero())
            , enableCompression(true)
            , enableEncryption(true)
            , enableResume(true)
            , verifyIntegrity(true)
            , maxConcurrentChunks(4)
            , bandwidthLimit(0)
            , retryCount(0)
        {
        }
    };

    // 传输进度事件参数 / Transfer Progress Event Arguments
    struct TransferProgressEventArgs
    {
        std::wstring transferId;
        uint64_t transferredBytes;
        uint64_t totalBytes;
        double percentage;
        double speed;
        std::chrono::milliseconds eta;
    };

    // 文件传输引擎类 / File Transfer Engine Class
    class FileTransferEngine
    {
    public:
        FileTransferEngine();
        ~FileTransferEngine();

        // 初始化 / Initialize
        winrt::Windows::Foundation::IAsyncOperation<bool> InitializeAsync();

        // 传输管理 / Transfer Management
        winrt::Windows::Foundation::IAsyncOperation<std::wstring> StartFileTransferAsync(
            const std::wstring& peerId,
            winrt::Windows::Storage::StorageFile const& file,
            TransferDirection direction);

        winrt::Windows::Foundation::IAsyncOperation<std::wstring> StartMultiFileTransferAsync(
            const std::wstring& peerId,
            winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Storage::StorageFile> files,
            TransferDirection direction);

        winrt::Windows::Foundation::IAsyncAction PauseTransferAsync(const std::wstring& transferId);
        winrt::Windows::Foundation::IAsyncAction ResumeTransferAsync(const std::wstring& transferId);
        winrt::Windows::Foundation::IAsyncAction CancelTransferAsync(const std::wstring& transferId);

        // 传输查询 / Transfer Query
        std::vector<std::shared_ptr<TransferSession>> GetActiveTransfers() const;
        std::shared_ptr<TransferSession> GetTransfer(const std::wstring& transferId) const;
        TransferStatus GetTransferStatus(const std::wstring& transferId) const;

        // 传输设置 / Transfer Settings
        void SetDefaultChunkSize(uint32_t chunkSize);
        void SetMaxConcurrentTransfers(uint32_t maxTransfers);
        void SetBandwidthLimit(uint64_t bytesPerSecond);
        void SetCompressionEnabled(bool enabled);
        void SetEncryptionEnabled(bool enabled);

        // 断点续传 / Resume Support
        winrt::Windows::Foundation::IAsyncOperation<bool> SaveTransferStateAsync(const std::wstring& transferId);
        winrt::Windows::Foundation::IAsyncOperation<bool> LoadTransferStateAsync(const std::wstring& transferId);
        winrt::Windows::Foundation::IAsyncAction CleanupIncompleteTransfersAsync();

        // 完整性校验 / Integrity Verification
        winrt::Windows::Foundation::IAsyncOperation<bool> VerifyFileIntegrityAsync(
            winrt::Windows::Storage::StorageFile const& file,
            const std::vector<uint8_t>& expectedHash);

        winrt::Windows::Foundation::IAsyncOperation<std::vector<uint8_t>> CalculateFileHashAsync(
            winrt::Windows::Storage::StorageFile const& file);

        // 事件处理 / Event Handling
        winrt::event_token TransferStarted(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler);
        void TransferStarted(winrt::event_token const& token) noexcept;

        winrt::event_token TransferProgress(winrt::Windows::Foundation::EventHandler<TransferProgressEventArgs> const& handler);
        void TransferProgress(winrt::event_token const& token) noexcept;

        winrt::event_token TransferCompleted(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler);
        void TransferCompleted(winrt::event_token const& token) noexcept;

        winrt::event_token TransferFailed(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler);
        void TransferFailed(winrt::event_token const& token) noexcept;

        // 统计信息 / Statistics
        struct TransferStatistics
        {
            uint32_t activeTransfers;
            uint32_t completedTransfers;
            uint32_t failedTransfers;
            uint64_t totalBytesTransferred;
            double averageTransferSpeed;
            std::chrono::milliseconds totalTransferTime;
        };

        TransferStatistics GetStatistics() const;

    private:
        // 内部传输管理 / Internal Transfer Management
        winrt::Windows::Foundation::IAsyncAction ProcessTransferAsync(std::shared_ptr<TransferSession> session);
        winrt::Windows::Foundation::IAsyncAction ProcessUploadAsync(std::shared_ptr<TransferSession> session);
        winrt::Windows::Foundation::IAsyncAction ProcessDownloadAsync(std::shared_ptr<TransferSession> session);

        // 分块处理 / Chunk Processing
        winrt::Windows::Foundation::IAsyncAction InitializeChunksAsync(std::shared_ptr<TransferSession> session);
        winrt::Windows::Foundation::IAsyncAction TransferChunkAsync(
            std::shared_ptr<TransferSession> session,
            uint32_t chunkIndex);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> ReadChunkAsync(
            winrt::Windows::Storage::StorageFile const& file,
            uint64_t offset,
            uint32_t size);

        winrt::Windows::Foundation::IAsyncAction WriteChunkAsync(
            winrt::Windows::Storage::StorageFile const& file,
            uint64_t offset,
            winrt::Windows::Storage::Streams::IBuffer const& data);

        // 数据压缩和加密 / Data Compression and Encryption
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> CompressDataAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> DecompressDataAsync(
            winrt::Windows::Storage::Streams::IBuffer const& compressedData);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> EncryptDataAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> DecryptDataAsync(
            winrt::Windows::Storage::Streams::IBuffer const& encryptedData);

        // 传输协议 / Transfer Protocol
        enum class TransferMessageType : uint8_t
        {
            TransferRequest = 0x10,
            TransferResponse = 0x11,
            ChunkRequest = 0x12,
            ChunkData = 0x13,
            ChunkAck = 0x14,
            TransferComplete = 0x15,
            TransferCancel = 0x16,
            TransferPause = 0x17,
            TransferResume = 0x18
        };

        struct TransferMessage
        {
            TransferMessageType type;
            std::wstring transferId;
            uint32_t chunkIndex;
            winrt::Windows::Storage::Streams::IBuffer data;
        };

        winrt::Windows::Foundation::IAsyncAction SendTransferMessageAsync(
            const std::wstring& peerId,
            const TransferMessage& message);

        winrt::Windows::Foundation::IAsyncAction HandleTransferMessageAsync(
            const std::wstring& peerId,
            const TransferMessage& message);

        // 进度跟踪 / Progress Tracking
        winrt::Windows::Foundation::IAsyncAction UpdateTransferProgressAsync(std::shared_ptr<TransferSession> session);
        void CalculateTransferSpeed(std::shared_ptr<TransferSession> session);
        void CalculateETA(std::shared_ptr<TransferSession> session);

        // 错误处理和重试 / Error Handling and Retry
        winrt::Windows::Foundation::IAsyncAction HandleTransferErrorAsync(
            std::shared_ptr<TransferSession> session,
            const std::wstring& error);

        winrt::Windows::Foundation::IAsyncOperation<bool> RetryChunkTransferAsync(
            std::shared_ptr<TransferSession> session,
            uint32_t chunkIndex);

        // 状态持久化 / State Persistence
        winrt::Windows::Foundation::IAsyncAction SaveSessionStateAsync(std::shared_ptr<TransferSession> session);
        winrt::Windows::Foundation::IAsyncOperation<std::shared_ptr<TransferSession>> LoadSessionStateAsync(
            const std::wstring& transferId);

        // 实用方法 / Utility Methods
        std::wstring GenerateTransferId();
        std::wstring GenerateSessionId();
        winrt::Windows::Foundation::IAsyncOperation<std::vector<uint8_t>> CalculateHashAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data);

    private:
        // 传输会话管理 / Transfer Session Management
        std::unordered_map<std::wstring, std::shared_ptr<TransferSession>> m_activeSessions;
        mutable std::mutex m_sessionsMutex;

        // 配置参数 / Configuration Parameters
        uint32_t m_defaultChunkSize;
        uint32_t m_maxConcurrentTransfers;
        uint64_t m_bandwidthLimit;
        bool m_compressionEnabled;
        bool m_encryptionEnabled;

        // 统计信息 / Statistics
        TransferStatistics m_statistics;
        mutable std::mutex m_statisticsMutex;

        // 事件 / Events
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_transferStartedEvent;
        winrt::event<winrt::Windows::Foundation::EventHandler<TransferProgressEventArgs>> m_transferProgressEvent;
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_transferCompletedEvent;
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_transferFailedEvent;

        // 带宽限制 / Bandwidth Limiting
        std::atomic<uint64_t> m_bytesTransferredThisSecond;
        std::chrono::steady_clock::time_point m_lastBandwidthCheck;

        // 定时器 / Timers
        winrt::Windows::System::Threading::ThreadPoolTimer m_progressTimer;
        winrt::Windows::System::Threading::ThreadPoolTimer m_cleanupTimer;

        // 常量 / Constants
        static constexpr uint32_t DEFAULT_CHUNK_SIZE = 1024 * 1024;      // 1MB
        static constexpr uint32_t MAX_CHUNK_SIZE = 16 * 1024 * 1024;     // 16MB
        static constexpr uint32_t MIN_CHUNK_SIZE = 64 * 1024;            // 64KB
        static constexpr uint32_t DEFAULT_MAX_CONCURRENT = 10;
        static constexpr uint32_t PROGRESS_UPDATE_INTERVAL_MS = 1000;    // 1秒 / 1 second
        static constexpr uint32_t CLEANUP_INTERVAL_MS = 300000;          // 5分钟 / 5 minutes
        static constexpr uint32_t MAX_RETRY_COUNT = 3;
    };
}