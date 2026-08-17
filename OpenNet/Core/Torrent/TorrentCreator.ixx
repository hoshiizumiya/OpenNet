export module OpenNet.Core.Torrent.TorrentCreator;

import std;

export namespace OpenNet::Core::Torrent
{
	enum class TorrentFormat
	{
		Hybrid,
		V1,
		V2,
	};

	struct TorrentCreationOptions
	{
		std::filesystem::path sourcePath;
		TorrentFormat format{ TorrentFormat::Hybrid };
		int pieceSize{};
		bool privateTorrent{};
		bool ignoreDotFiles{ true };
		std::string comment;
		std::vector<std::string> trackers;
		std::vector<std::string> urlSeeds;
	};

	struct TorrentCreationResult
	{
		std::vector<std::uint8_t> data;
		int pieceSize{};
	};

	class TorrentCreator final
	{
	public:
		static TorrentCreationResult Create(TorrentCreationOptions const& options);
		static void WriteFile(
			std::filesystem::path const& target,
			TorrentCreationResult const& result);
	};
}
