#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace OpenNet::Core::Notification
{
	void ShowHttpDownloadCompleted(std::string const& name, std::filesystem::path const& outputPath, std::int64_t elapsedSeconds, std::uint64_t completedBytes);
}
