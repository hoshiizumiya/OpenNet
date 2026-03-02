/*
 * PROJECT:   OpenNet
 * FILE:      Core/Aria2/Aria2Models.h
 * PURPOSE:   Aria2 data model definitions
 *            (migrated from NanaGet.Aria2.h)
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace OpenNet::Core::Aria2
{
    bool ToBoolean(nlohmann::json const &Value);

    using DownloadGid = std::uint64_t;

    std::string FromDownloadGid(DownloadGid const &Value);
    DownloadGid ToDownloadGid(nlohmann::json const &Value);

    enum class DownloadStatus : std::int32_t
    {
        Active,
        Waiting,
        Paused,
        Complete,
        Error,
        Removed,
    };

    DownloadStatus ToDownloadStatus(nlohmann::json const &Value);

    enum class UriStatus : std::int32_t
    {
        Used,
        Waiting,
    };

    UriStatus ToUriStatus(nlohmann::json const &Value);

    struct UriInformation
    {
        std::string Uri;
        UriStatus Status = UriStatus::Used;
    };

    UriInformation ToUriInformation(nlohmann::json const &Value);

    struct FileInformation
    {
        std::size_t Index = 0;
        std::string Path;
        std::size_t Length = 0;
        std::size_t CompletedLength = 0;
        bool Selected = true;
        std::vector<UriInformation> Uris;
    };

    FileInformation ToFileInformation(nlohmann::json const &Value);

    enum class BitTorrentFileMode : std::int32_t
    {
        None,
        Single,
        Multi,
    };

    BitTorrentFileMode ToBitTorrentFileMode(nlohmann::json const &Value);

    struct BitTorrentInfoDictionary
    {
        std::string Name;
    };

    BitTorrentInfoDictionary ToBitTorrentInfoDictionary(nlohmann::json const &Value);

    struct BitTorrentInformation
    {
        std::vector<std::vector<std::string>> AnnounceList;
        std::string Comment;
        std::time_t CreationDate = 0;
        BitTorrentFileMode Mode = BitTorrentFileMode::None;
        BitTorrentInfoDictionary Info;
    };

    BitTorrentInformation ToBitTorrentInformation(nlohmann::json const &Value);

    struct DownloadInformation
    {
        DownloadGid Gid = 0;
        DownloadStatus Status = DownloadStatus::Error;
        std::size_t TotalLength = 0;
        std::size_t CompletedLength = 0;
        std::size_t UploadLength = 0;
        std::string Bitfield;
        std::size_t DownloadSpeed = 0;
        std::size_t UploadSpeed = 0;
        std::string InfoHash;
        std::size_t NumSeeders = 0;
        bool Seeder = false;
        std::size_t PieceLength = 0;
        std::size_t NumPieces = 0;
        std::int32_t Connections = 0;
        std::int32_t ErrorCode = 0;
        std::string ErrorMessage;
        std::vector<DownloadGid> FollowedBy;
        DownloadGid Following = 0;
        DownloadGid BelongsTo = 0;
        std::string Dir;
        std::vector<FileInformation> Files;
        BitTorrentInformation BitTorrent;
        std::size_t VerifiedLength = 0;
        bool VerifyIntegrityPending = false;
    };

    DownloadInformation ToDownloadInformation(nlohmann::json const &Value);
    std::string ToFriendlyName(DownloadInformation const &Value);

    struct PeerInformation
    {
        std::string PeerId;
        std::string Ip;
        std::uint16_t Port = 0;
        std::string Bitfield;
        bool AmChoking = false;
        bool PeerChoking = false;
        std::size_t DownloadSpeed = 0;
        std::size_t UploadSpeed = 0;
        bool Seeder = false;
    };

    PeerInformation ToPeerInformation(nlohmann::json const &Value);

    struct ServerInformation
    {
        std::string Uri;
        std::string CurrentUri;
        std::size_t DownloadSpeed = 0;
    };

    ServerInformation ToServerInformation(nlohmann::json const &Value);

    struct ServersInformation
    {
        std::size_t Index = 0;
        std::vector<ServerInformation> Servers;
    };

    ServersInformation ToServersInformation(nlohmann::json const &Value);

    struct GlobalStatusInformation
    {
        std::size_t DownloadSpeed = 0;
        std::size_t UploadSpeed = 0;
        std::size_t NumActive = 0;
        std::size_t NumWaiting = 0;
        std::size_t NumStopped = 0;
        std::size_t NumStoppedTotal = 0;
    };

    GlobalStatusInformation ToGlobalStatusInformation(nlohmann::json const &Value);

    struct VersionInformation
    {
        std::string Version;
        std::vector<std::string> EnabledFeatures;
    };

    VersionInformation ToVersionInformation(nlohmann::json const &Value);

    struct SessionInformation
    {
        std::string SessionId;
    };

    SessionInformation ToSessionInformation(nlohmann::json const &Value);
}
