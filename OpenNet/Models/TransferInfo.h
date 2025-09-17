#pragma once

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>

namespace winrt::OpenNet::Models
{
	// 简化的文件信息模型 / Simplified File Information Model
	struct OpenNet
	{
		// 基本文件信息 / Basic File Information
		winrt::hstring fileName;             // 文件名 / File Name
		winrt::hstring filePath;             // 文件路径 / File Path
		winrt::hstring relativePath;         // 相对路径 / Relative Path
		uint64_t fileSize;                 // 文件大小 / File Size
		winrt::hstring mimeType;             // MIME类型 / MIME Type
		winrt::Windows::Foundation::DateTime lastModified; // 最后修改时间 / Last Modified

		// 文件完整性 / File Integrity
		winrt::hstring md5Hash;              // MD5哈希 / MD5 Hash
		winrt::hstring sha256Hash;           // SHA256哈希 / SHA256 Hash
		winrt::hstring crc32;                // CRC32校验 / CRC32 Checksum

		// 文件元数据 / File Metadata
		winrt::hstring description;      // 文件描述 / File Description
		winrt::hstring category;         // 文件类别 / File Category
		winrt::hstring author;           // 作者 / Author
		winrt::hstring version;          // 版本 / Version
		bool isExecutable;             // 是否可执行 / Is Executable
		bool isCompressed;             // 是否压缩 / Is Compressed
		bool isEncrypted;              // 是否加密 / Is Encrypted

		// 构造函数 / Constructor
		OpenNet()
			: fileSize(0)
			, lastModified(winrt::clock::now())
			, isExecutable(false)
			, isCompressed(false)
			, isEncrypted(false)
		{
		}

		// 实用方法 / Utility Methods
		winrt::hstring GetFileExtension() const
		{
			std::wstring name{ fileName.c_str() };
			auto dotPos = name.find_last_of(L'.');
			if (dotPos != std::wstring::npos)
				return winrt::hstring{ name.substr(dotPos) };
			return L"";
		}

		winrt::hstring GetFileSizeString() const
		{
			return FormatBytes(fileSize);
		}

		bool IsImageFile() const
		{
			auto ext = GetFileExtension();
			std::wstring extStr{ ext.c_str() };
			return extStr == L".jpg" || extStr == L".jpeg" || extStr == L".png" ||
				extStr == L".gif" || extStr == L".bmp" || extStr == L".tiff";
		}

		bool IsVideoFile() const
		{
			auto ext = GetFileExtension();
			std::wstring extStr{ ext.c_str() };
			return extStr == L".mp4" || extStr == L".avi" || extStr == L".mkv" ||
				extStr == L".mov" || extStr == L".wmv" || extStr == L".flv";
		}

		bool IsAudioFile() const
		{
			auto ext = GetFileExtension();
			std::wstring extStr{ ext.c_str() };
			return extStr == L".mp3" || extStr == L".wav" || extStr == L".flac" ||
				extStr == L".aac" || extStr == L".ogg" || extStr == L".wma";
		}

		// 格式化字节数 / Format Bytes
		static winrt::hstring FormatBytes(uint64_t bytes)
		{
			const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
			int unitIndex = 0;
			double size = static_cast<double>(bytes);

			while (size >= 1024.0 && unitIndex < 4)
			{
				size /= 1024.0;
				unitIndex++;
			}

			wchar_t buffer[64];
			if (unitIndex == 0)
				swprintf_s(buffer, L"%.0f %s", size, units[unitIndex]);
			else
				swprintf_s(buffer, L"%.2f %s", size, units[unitIndex]);

			return winrt::hstring{ buffer };
		}
	};

	// 简化的传输信息模型 / Simplified Transfer Information Model
	struct TransferInfo
	{
		// 基本传输信息 / Basic Transfer Information
		winrt::hstring transferId;           // 传输ID / Transfer ID
		winrt::hstring sessionId;            // 会话ID / Session ID
		winrt::hstring peerId;               // 对等节点ID / Peer ID
		winrt::hstring peerName;             // 对等节点名称 / Peer Name

		// 传输方向 / Transfer Direction
		enum class Direction
		{
			Upload,                        // 上传 / Upload
			Download                       // 下载 / Download
		} direction;

		// 传输状态 / Transfer Status
		enum class Status
		{
			Pending,                       // 等待中 / Pending
			Initializing,                  // 初始化中 / Initializing
			Transferring,                  // 传输中 / Transferring
			Paused,                        // 已暂停 / Paused
			Completed,                     // 已完成 / Completed
			Failed,                        // 失败 / Failed
			Cancelled,                     // 已取消 / Cancelled
			Verifying                      // 校验中 / Verifying
		} status;

		// 文件信息 / File Information
		OpenNet OpenNet;                // 文件信息 / File Information
		uint64_t totalSize;                // 总大小 / Total Size
		uint64_t transferredSize;          // 已传输大小 / Transferred Size

		// 传输统计 / Transfer Statistics
		double transferSpeed;              // 传输速度(字节/秒) / Transfer Speed (bytes/sec)
		double averageSpeed;               // 平均速度 / Average Speed
		winrt::Windows::Foundation::DateTime startTime;      // 开始时间 / Start Time
		winrt::Windows::Foundation::DateTime endTime;        // 结束时间 / End Time

		// 传输选项 / Transfer Options
		bool enableCompression;            // 启用压缩 / Enable Compression
		bool enableEncryption;             // 启用加密 / Enable Encryption
		bool enableResume;                 // 启用断点续传 / Enable Resume
		bool verifyIntegrity;              // 验证完整性 / Verify Integrity
		uint32_t maxConcurrentChunks;      // 最大并发分块数 / Max Concurrent Chunks
		uint64_t bandwidthLimit;           // 带宽限制 / Bandwidth Limit

		// 错误信息 / Error Information
		winrt::hstring lastError;          // 最后错误 / Last Error
		uint32_t retryCount;               // 重试次数 / Retry Count
		uint32_t maxRetries;               // 最大重试次数 / Max Retries

		// 构造函数 / Constructor
		TransferInfo()
			: direction(Direction::Download)
			, status(Status::Pending)
			, totalSize(0)
			, transferredSize(0)
			, transferSpeed(0.0)
			, averageSpeed(0.0)
			, enableCompression(true)
			, enableEncryption(true)
			, enableResume(true)
			, verifyIntegrity(true)
			, maxConcurrentChunks(4)
			, bandwidthLimit(0)
			, retryCount(0)
			, maxRetries(3)
		{
			startTime = {};
			endTime = {};
		}

		// 获取传输状态字符串 / Get Transfer Status String
		winrt::hstring GetStatusString() const
		{
			switch (status)
			{
			case Status::Pending: return L"等待中 / Pending";
			case Status::Initializing: return L"初始化中 / Initializing";
			case Status::Transferring: return L"传输中 / Transferring";
			case Status::Paused: return L"已暂停 / Paused";
			case Status::Completed: return L"已完成 / Completed";
			case Status::Failed: return L"失败 / Failed";
			case Status::Cancelled: return L"已取消 / Cancelled";
			case Status::Verifying: return L"校验中 / Verifying";
			default: return L"未知状态 / Unknown";
			}
		}

		// 获取传输进度百分比 / Get Transfer Progress Percentage
		double GetProgress() const
		{
			if (totalSize == 0) return 0.0;
			return static_cast<double>(transferredSize) / static_cast<double>(totalSize) * 100.0;
		}

		// 获取传输速度字符串 / Get Transfer Speed String
		winrt::hstring GetSpeedString() const
		{
			return OpenNet::FormatBytes(static_cast<uint64_t>(transferSpeed)) + L"/s";
		}

		// 获取平均速度字符串 / Get Average Speed String
		winrt::hstring GetAverageSpeedString() const
		{
			return OpenNet::FormatBytes(static_cast<uint64_t>(averageSpeed)) + L"/s";
		}
	};
}
