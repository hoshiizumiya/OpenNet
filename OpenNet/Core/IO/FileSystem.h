#pragma once
#include <string>
#include <vector>

namespace winrt::OpenNet::Core::IO
{
    class FileSystem
    {
    public:
        // Wide-string variants for callers that need std::wstring
        static std::wstring_view GetAppDataPathW();
        static std::wstring_view GetAppTempPathW();
        static std::wstring_view GetDownloadsPathW();

        // Create directory if not exists
        static bool CreateDirectory(const std::wstring& path);

        // Check if directory exists
        static bool DirectoryExists(const std::wstring& path);

        // Check if file exists
        static bool FileExists(const std::wstring& path);

    private:
        static std::wstring_view AppDataPathW;
        static std::wstring_view AppTempPathW;
        static std::wstring_view AppDownloadPathW;
    };
}
