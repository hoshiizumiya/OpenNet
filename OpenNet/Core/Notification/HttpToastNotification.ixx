export module OpenNet.Core.Notification.HttpToastNotification;

export import std;

export namespace OpenNet::Core::Notification
{
	void ShowHttpDownloadCompleted(std::string const& name, std::filesystem::path const& outputPath, std::int64_t elapsedSeconds, std::uint64_t completedBytes);
	void ShowTorrentDownloadCompleted(std::string const& name, std::filesystem::path const& savePath);
	bool ShowTestNotification() noexcept;
}
