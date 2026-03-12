#pragma once
#include <string>
#include <functional>

namespace winrt::OpenNet::Core::IO
{
    class FileOperation
    {
    public:
        // Move directory to new location
        // callback: receives (current_step, total_steps) for progress
        static bool MoveDirectory(
            const std::wstring& sourcePath,
            const std::wstring& destinationPath,
            std::function<void(size_t, size_t)> progressCallback = nullptr);

        // Copy directory to new location
        static bool CopyDirectory(
            const std::wstring& sourcePath,
            const std::wstring& destinationPath,
            std::function<void(size_t, size_t)> progressCallback = nullptr);

        // Delete directory recursively
        static bool DeleteDirectory(const std::wstring& path);

        // Get total size of directory
        static uint64_t GetDirectorySize(const std::wstring& path);

        // Get available free space
        static uint64_t GetAvailableSpace(const std::wstring& path);
    };
}
