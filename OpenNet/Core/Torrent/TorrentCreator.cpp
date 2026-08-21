module;

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "LibtorrentIncludeGuard.h"
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/pread_disk_io.hpp>
#include <libtorrent/settings_pack.hpp>
#include "LibtorrentIncludeRestore.h"

#include <Windows.h>

module OpenNet.Core.Torrent.TorrentCreator;

import std;
import OpenNet.Core.TorrentSettings;

namespace OpenNet::Core::Torrent
{
	namespace
	{
		std::string PathUtf8(std::filesystem::path const& path)
		{
			auto const wide = path.wstring();
			if (wide.empty()) return {};
			auto const size = WideCharToMultiByte(
				CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
				static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
			if (size <= 0) throw std::runtime_error("Unable to encode the path as UTF-8");
			std::string result(static_cast<std::size_t>(size), '\0');
			WideCharToMultiByte(
				CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
				static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr);
			return result;
		}

		bool IsDotPath(std::string const& value)
		{
			for (auto const& component : std::filesystem::path(value))
			{
				auto const name = component.string();
				if (name.size() > 1 && name.front() == '.'
					&& name != "." && name != "..")
				{
					return true;
				}
			}
			return false;
		}
	}

	TorrentCreationResult TorrentCreator::Create(
		TorrentCreationOptions const& options)
	{
		std::error_code filesystemError;
		if (options.sourcePath.empty()
			|| !std::filesystem::exists(options.sourcePath, filesystemError)
			|| filesystemError)
		{
			throw std::invalid_argument("The torrent source path does not exist");
		}

		libtorrent::create_flags_t flags{};
		switch (options.format)
		{
			case TorrentFormat::V1:
				flags |= libtorrent::create_torrent::v1_only;
				break;
			case TorrentFormat::V2:
				flags |= libtorrent::create_torrent::v2_only;
				break;
			case TorrentFormat::Hybrid:
				break;
		}

		auto const source = PathUtf8(options.sourcePath);
		auto files = options.ignoreDotFiles
			? libtorrent::list_files(
				source,
				[](std::string const& path)
		{
			return !IsDotPath(path);
		},
				flags)
			: libtorrent::list_files(source, flags);
		if (files.empty()) throw std::runtime_error("No files were found in the source path");

		libtorrent::create_torrent creator(
			std::move(files), (std::max)(0, options.pieceSize), flags);
		creator.set_priv(options.privateTorrent);
		if (!options.comment.empty()) creator.set_comment(options.comment.c_str());
		creator.set_creator("OpenNet");
		int tier{};
		for (auto const& tracker : options.trackers)
		{
			if (!tracker.empty()) creator.add_tracker(tracker, tier++);
		}
		for (auto const& seed : options.urlSeeds)
		{
			if (!seed.empty()) creator.add_url_seed(seed);
		}

		libtorrent::error_code error;
		auto const torrentSettings = ::OpenNet::Core::TorrentSettingsManager::Instance().Get();
		libtorrent::settings_pack hashingSettings;
		hashingSettings.set_int(libtorrent::settings_pack::aio_threads, std::max(1, torrentSettings.aioThreads));
		hashingSettings.set_int(libtorrent::settings_pack::hashing_threads, std::max(1, torrentSettings.hashingThreads));
		libtorrent::set_piece_hashes(creator, PathUtf8(options.sourcePath.parent_path()), hashingSettings, libtorrent::pread_disk_io_constructor, [](libtorrent::piece_index_t)
		{}, error);
		if (error) throw std::runtime_error(error.message());

		std::vector<char> encoded;
		libtorrent::bencode(std::back_inserter(encoded), creator.generate());
		TorrentCreationResult result;
		result.data.assign(encoded.begin(), encoded.end());
		result.pieceSize = creator.piece_length();
		return result;
	}

	void TorrentCreator::WriteFile(
		std::filesystem::path const& target,
		TorrentCreationResult const& result)
	{
		if (target.empty()) throw std::invalid_argument("The output path is empty");
		std::filesystem::create_directories(target.parent_path());
		auto temporary = target;
		temporary += L".tmp";
		try
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			if (!output) throw std::runtime_error("Unable to open the output file");
			output.write(
				reinterpret_cast<char const*>(result.data.data()),
				static_cast<std::streamsize>(result.data.size()));
			output.close();
			if (!output) throw std::runtime_error("Unable to write the torrent file");
			if (!MoveFileExW(
				temporary.c_str(), target.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				auto const error = std::error_code(
					static_cast<int>(GetLastError()), std::system_category());
				throw std::filesystem::filesystem_error(
					"Unable to commit the torrent file", temporary, target, error);
			}
		}
		catch (...)
		{
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			throw;
		}
	}
}
