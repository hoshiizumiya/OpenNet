#pragma once
#include <string>
#include <vector>

namespace winrt::OpenNet::Core::IO
{
    class FileSystem
    {
    public:
        // Get AppData folder path (where SQLite database is stored)
        // Returns path to %APPDATA%\OpenNet
        static std::string GetAppDataPath();

        // Get application temporary folder
        static std::string GetTempPath();

        // Get downloads folder
        static std::string GetDownloadsPath();

        // Create directory if not exists
        static bool CreateDirectory(const std::string& path);

        // Check if directory exists
        static bool DirectoryExists(const std::string& path);

        // Check if file exists
        static bool FileExists(const std::string& path);
    };
}
