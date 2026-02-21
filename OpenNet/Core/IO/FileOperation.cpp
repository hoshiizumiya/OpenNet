#include "pch.h"
#include "FileOperation.h"
#include "FileSystem.h"
#include <windows.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace winrt::OpenNet::Core::IO
{
	bool FileOperation::MoveDirectory(
		const std::string& sourcePath,
		const std::string& destinationPath,
		std::function<void(size_t, size_t)> progressCallback)
	{
		try
		{
			if (!FileSystem::DirectoryExists(sourcePath))
			{
				return false;
			}

			// Ensure destination parent exists
			auto destParent = fs::path(destinationPath).parent_path();
			if (!destParent.empty())
			{
				FileSystem::CreateDirectory(destParent.string());
			}

			// Convert to wide strings
			auto wSource = fs::path(sourcePath).wstring();
			auto wDest = fs::path(destinationPath).wstring();

			// Use MoveFileEx for atomic operation
			if (MoveFileExW(wSource.c_str(), wDest.c_str(), MOVEFILE_REPLACE_EXISTING))
			{
				if (progressCallback)
				{
					progressCallback(1, 1);
				}
				return true;
			}

			return false;
		}
		catch (...)
		{
			return false;
		}
	}

	bool FileOperation::CopyDirectory(
		const std::string& sourcePath,
		const std::string& destinationPath,
		std::function<void(size_t, size_t)> progressCallback)
	{
		try
		{
			if (!FileSystem::DirectoryExists(sourcePath))
			{
				return false;
			}

			// Ensure destination parent exists
			auto destParent = fs::path(destinationPath).parent_path();
			if (!destParent.empty())
			{
				FileSystem::CreateDirectory(destParent.string());
			}

			// Copy recursively
			size_t fileCount = 0;
			size_t processedCount = 0;

			// First pass: count files
			for (const auto& entry : fs::recursive_directory_iterator(sourcePath))
			{
				if (entry.is_regular_file())
				{
					fileCount++;
				}
			}

			// Second pass: copy files
			for (const auto& entry : fs::recursive_directory_iterator(sourcePath))
			{
				if (entry.is_regular_file())
				{
					auto relativePath = fs::relative(entry.path(), sourcePath);
					auto destFile = fs::path(destinationPath) / relativePath;

					// Create directory if needed
					FileSystem::CreateDirectory(destFile.parent_path().string());

					// Copy file
					fs::copy_file(entry.path(), destFile, fs::copy_options::overwrite_existing);

					processedCount++;
					if (progressCallback)
					{
						progressCallback(processedCount, fileCount);
					}
				}
			}

			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool FileOperation::DeleteDirectory(const std::string& path)
	{
		try
		{
			if (!FileSystem::DirectoryExists(path))
			{
				return false;
			}

			// Use filesystem to recursively delete
			size_t removed = fs::remove_all(path);
			return removed > 0;
		}
		catch (...)
		{
			return false;
		}
	}

	uint64_t FileOperation::GetDirectorySize(const std::string& path)
	{
		try
		{
			if (!FileSystem::DirectoryExists(path))
			{
				return 0;
			}

			uint64_t totalSize = 0;
			for (const auto& entry : fs::recursive_directory_iterator(path))
			{
				if (entry.is_regular_file())
				{
					totalSize += entry.file_size();
				}
			}

			return totalSize;
		}
		catch (...)
		{
			return 0;
		}
	}

	uint64_t FileOperation::GetAvailableSpace(const std::string& path)
	{
		try
		{
			auto wpath = fs::path(path).wstring();
			
			ULARGE_INTEGER freeBytes;
			ULARGE_INTEGER totalBytes;
			ULARGE_INTEGER totalFreeBytes;

			if (GetDiskFreeSpaceExW(wpath.c_str(), &freeBytes, &totalBytes, &totalFreeBytes))
			{
				return freeBytes.QuadPart;
			}

			return 0;
		}
		catch (...)
		{
			return 0;
		}
	}
}
