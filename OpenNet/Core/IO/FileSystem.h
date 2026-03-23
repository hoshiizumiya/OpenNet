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
        static winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> GetDownloadsPathW();

        // Create directory if not exists
        static bool CreateDirectory(const std::wstring& path);

        // Check if directory exists
        static bool DirectoryExists(const std::wstring& path);

        // Check if file exists
        static bool FileExists(const std::wstring& path);

    private:
        static std::wstring AppDataPathW;
        static std::wstring AppTempPathW;
        static winrt::hstring AppDownloadPathW;
    };
}
