/*
 * PROJECT:   OpenNet
 * FILE:      Core/Aria2/Aria2Models.cpp
 * PURPOSE:   Aria2 data model implementations
 *            (migrated from NanaGet.Aria2.cpp)
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/Aria2/Aria2Models.h"
#include "Core/Aria2/Aria2Helpers.h"

using namespace OpenNet::Core::Aria2::Helpers;

bool OpenNet::Core::Aria2::ToBoolean(nlohmann::json const &Value)
{
    std::string raw = JsonToString(Value);
    return (raw == "true");
}

std::string OpenNet::Core::Aria2::FromDownloadGid(DownloadGid const &Value)
{
    return FormatString("%016llX", Value);
}

OpenNet::Core::Aria2::DownloadGid OpenNet::Core::Aria2::ToDownloadGid(nlohmann::json const &Value)
{
    return ToUInt64(JsonToString(Value), 16);
}

OpenNet::Core::Aria2::DownloadStatus OpenNet::Core::Aria2::ToDownloadStatus(nlohmann::json const &Value)
{
    std::string raw = JsonToString(Value);
    if (raw == "active")
        return DownloadStatus::Active;
    if (raw == "waiting")
        return DownloadStatus::Waiting;
    if (raw == "paused")
        return DownloadStatus::Paused;
    if (raw == "complete")
        return DownloadStatus::Complete;
    if (raw == "removed")
        return DownloadStatus::Removed;
    return DownloadStatus::Error;
}

OpenNet::Core::Aria2::UriStatus OpenNet::Core::Aria2::ToUriStatus(nlohmann::json const &Value)
{
    std::string raw = JsonToString(Value);
    if (raw == "waiting")
        return UriStatus::Waiting;
    return UriStatus::Used;
}

OpenNet::Core::Aria2::UriInformation OpenNet::Core::Aria2::ToUriInformation(nlohmann::json const &Value)
{
    UriInformation result;
    result.Uri = JsonToString(JsonGetSubKey(Value, "uri"));
    result.Status = ToUriStatus(JsonToPrimitive(JsonGetSubKey(Value, "status")));
    return result;
}

OpenNet::Core::Aria2::FileInformation OpenNet::Core::Aria2::ToFileInformation(nlohmann::json const &Value)
{
    FileInformation result;
    result.Index = ToUInt64(JsonToString(JsonGetSubKey(Value, "index")));
    result.Path = JsonToString(JsonGetSubKey(Value, "path"));
    result.Length = ToUInt64(JsonToString(JsonGetSubKey(Value, "length")));
    result.CompletedLength = ToUInt64(JsonToString(JsonGetSubKey(Value, "completedLength")));
    result.Selected = ToBoolean(JsonGetSubKey(Value, "selected"));

    for (auto const &uri : JsonToArray(JsonGetSubKey(Value, "uris")))
    {
        result.Uris.emplace_back(ToUriInformation(JsonToObject(uri)));
    }
    return result;
}

OpenNet::Core::Aria2::BitTorrentFileMode OpenNet::Core::Aria2::ToBitTorrentFileMode(nlohmann::json const &Value)
{
    std::string raw = JsonToString(Value);
    if (raw == "single")
        return BitTorrentFileMode::Single;
    if (raw == "multi")
        return BitTorrentFileMode::Multi;
    return BitTorrentFileMode::None;
}

OpenNet::Core::Aria2::BitTorrentInfoDictionary OpenNet::Core::Aria2::ToBitTorrentInfoDictionary(nlohmann::json const &Value)
{
    BitTorrentInfoDictionary result;
    result.Name = JsonToString(JsonGetSubKey(Value, "name"));
    return result;
}

OpenNet::Core::Aria2::BitTorrentInformation OpenNet::Core::Aria2::ToBitTorrentInformation(nlohmann::json const &Value)
{
    BitTorrentInformation result;

    for (auto const &arr : JsonToArray(JsonGetSubKey(Value, "announceList")))
    {
        std::vector<std::string> content;
        for (auto const &item : JsonToArray(arr))
        {
            content.emplace_back(JsonToString(item));
        }
        result.AnnounceList.emplace_back(std::move(content));
    }

    result.Comment = JsonToString(JsonGetSubKey(Value, "comment"));
    result.CreationDate = static_cast<std::time_t>(
        ToUInt64(JsonToString(JsonGetSubKey(Value, "creationDate"))));
    result.Mode = ToBitTorrentFileMode(JsonToPrimitive(JsonGetSubKey(Value, "mode")));
    result.Info = ToBitTorrentInfoDictionary(JsonToObject(JsonGetSubKey(Value, "info")));

    return result;
}

OpenNet::Core::Aria2::DownloadInformation OpenNet::Core::Aria2::ToDownloadInformation(nlohmann::json const &Value)
{
    DownloadInformation result;

    result.Gid = ToDownloadGid(JsonToPrimitive(JsonGetSubKey(Value, "gid")));
    result.Status = ToDownloadStatus(JsonToPrimitive(JsonGetSubKey(Value, "status")));
    result.TotalLength = ToUInt64(JsonToString(JsonGetSubKey(Value, "totalLength")));
    result.CompletedLength = ToUInt64(JsonToString(JsonGetSubKey(Value, "completedLength")));
    result.UploadLength = ToUInt64(JsonToString(JsonGetSubKey(Value, "uploadLength")));
    result.Bitfield = JsonToString(JsonGetSubKey(Value, "bitfield"));
    result.DownloadSpeed = ToUInt64(JsonToString(JsonGetSubKey(Value, "downloadSpeed")));
    result.UploadSpeed = ToUInt64(JsonToString(JsonGetSubKey(Value, "uploadSpeed")));
    result.InfoHash = JsonToString(JsonGetSubKey(Value, "infoHash"));
    result.NumSeeders = ToUInt64(JsonToString(JsonGetSubKey(Value, "numSeeders")));
    result.Seeder = ToBoolean(JsonGetSubKey(Value, "seeder"));
    result.PieceLength = ToUInt64(JsonToString(JsonGetSubKey(Value, "pieceLength")));
    result.NumPieces = ToUInt64(JsonToString(JsonGetSubKey(Value, "numPieces")));
    result.Connections = ToInt32(JsonToString(JsonGetSubKey(Value, "connections")));
    result.ErrorCode = ToInt32(JsonToString(JsonGetSubKey(Value, "errorCode")));
    result.ErrorMessage = JsonToString(JsonGetSubKey(Value, "errorMessage"));

    for (auto const &item : JsonToArray(JsonGetSubKey(Value, "followedBy")))
    {
        result.FollowedBy.emplace_back(ToDownloadGid(item));
    }

    result.Following = ToDownloadGid(JsonToPrimitive(JsonGetSubKey(Value, "following")));
    result.BelongsTo = ToDownloadGid(JsonToPrimitive(JsonGetSubKey(Value, "belongsTo")));
    result.Dir = JsonToString(JsonGetSubKey(Value, "dir"));

    for (auto const &file : JsonToArray(JsonGetSubKey(Value, "files")))
    {
        result.Files.emplace_back(ToFileInformation(JsonToObject(file)));
    }

    result.BitTorrent = ToBitTorrentInformation(JsonToObject(JsonGetSubKey(Value, "bittorrent")));
    result.VerifiedLength = ToUInt64(JsonToString(JsonGetSubKey(Value, "verifiedLength")));
    result.VerifyIntegrityPending = ToBoolean(JsonGetSubKey(Value, "verifyIntegrityPending"));

    return result;
}

std::string OpenNet::Core::Aria2::ToFriendlyName(DownloadInformation const &Value)
{
    if (!Value.BitTorrent.Info.Name.empty())
    {
        return Value.BitTorrent.Info.Name;
    }

    if (!Value.Files.empty())
    {
        const char *candidate = nullptr;

        if (!Value.Files[0].Path.empty())
        {
            candidate = Value.Files[0].Path.c_str();
        }
        else if (!Value.Files[0].Uris.empty())
        {
            candidate = Value.Files[0].Uris[0].Uri.c_str();
        }

        if (candidate)
        {
            const char *rawName = std::strrchr(candidate, '/');
            return std::string(
                (rawName && rawName != candidate) ? &rawName[1] : candidate);
        }
    }

    return FromDownloadGid(Value.Gid);
}

OpenNet::Core::Aria2::PeerInformation OpenNet::Core::Aria2::ToPeerInformation(nlohmann::json const &Value)
{
    PeerInformation result;
    result.PeerId = JsonToString(JsonGetSubKey(Value, "peerId"));
    result.Ip = JsonToString(JsonGetSubKey(Value, "ip"));
    result.Port = static_cast<std::uint16_t>(
        ToUInt32(JsonToString(JsonGetSubKey(Value, "port"))));
    result.Bitfield = JsonToString(JsonGetSubKey(Value, "bitfield"));
    result.AmChoking = ToBoolean(JsonGetSubKey(Value, "amChoking"));
    result.PeerChoking = ToBoolean(JsonGetSubKey(Value, "peerChoking"));
    result.DownloadSpeed = ToUInt64(JsonToString(JsonGetSubKey(Value, "downloadSpeed")));
    result.UploadSpeed = ToUInt64(JsonToString(JsonGetSubKey(Value, "uploadSpeed")));
    result.Seeder = ToBoolean(JsonGetSubKey(Value, "seeder"));
    return result;
}

OpenNet::Core::Aria2::ServerInformation OpenNet::Core::Aria2::ToServerInformation(nlohmann::json const &Value)
{
    ServerInformation result;
    result.Uri = JsonToString(JsonGetSubKey(Value, "uri"));
    result.CurrentUri = JsonToString(JsonGetSubKey(Value, "currentUri"));
    result.DownloadSpeed = ToUInt64(JsonToString(JsonGetSubKey(Value, "downloadSpeed")));
    return result;
}

OpenNet::Core::Aria2::ServersInformation OpenNet::Core::Aria2::ToServersInformation(nlohmann::json const &Value)
{
    ServersInformation result;
    result.Index = ToUInt64(JsonToString(JsonGetSubKey(Value, "index")));
    for (auto const &server : JsonToArray(JsonGetSubKey(Value, "servers")))
    {
        result.Servers.emplace_back(ToServerInformation(JsonToObject(server)));
    }
    return result;
}

OpenNet::Core::Aria2::GlobalStatusInformation OpenNet::Core::Aria2::ToGlobalStatusInformation(nlohmann::json const &Value)
{
    GlobalStatusInformation result;
    result.DownloadSpeed = ToUInt64(JsonToString(JsonGetSubKey(Value, "downloadSpeed")));
    result.UploadSpeed = ToUInt64(JsonToString(JsonGetSubKey(Value, "uploadSpeed")));
    result.NumActive = ToUInt64(JsonToString(JsonGetSubKey(Value, "numActive")));
    result.NumWaiting = ToUInt64(JsonToString(JsonGetSubKey(Value, "numWaiting")));
    result.NumStopped = ToUInt64(JsonToString(JsonGetSubKey(Value, "numStopped")));
    result.NumStoppedTotal = ToUInt64(JsonToString(JsonGetSubKey(Value, "numStoppedTotal")));
    return result;
}

OpenNet::Core::Aria2::VersionInformation OpenNet::Core::Aria2::ToVersionInformation(nlohmann::json const &Value)
{
    VersionInformation result;
    result.Version = JsonToString(JsonGetSubKey(Value, "version"));
    for (auto const &feat : JsonToArray(JsonGetSubKey(Value, "enabledFeatures")))
    {
        result.EnabledFeatures.emplace_back(feat.get<std::string>());
    }
    return result;
}

OpenNet::Core::Aria2::SessionInformation OpenNet::Core::Aria2::ToSessionInformation(nlohmann::json const &Value)
{
    SessionInformation result;
    result.SessionId = JsonToString(JsonGetSubKey(Value, "sessionId"));
    return result;
}
