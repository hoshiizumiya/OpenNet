#pragma once
#include "ViewModels/PeerDisplayItem.g.h"
#include "ViewModels/TrackerDisplayItem.g.h"
#include "ViewModels/FileDisplayItem.g.h"

namespace winrt::OpenNet::ViewModels::implementation
{
    // ---------------------------------------------------------------
    // PeerDisplayItem
    // ---------------------------------------------------------------
    struct PeerDisplayItem : PeerDisplayItemT<PeerDisplayItem>
    {
        PeerDisplayItem() = default;

        winrt::hstring IP() const { return m_ip; }
        void IP(winrt::hstring const& v) { m_ip = v; }

        winrt::hstring Client() const { return m_client; }
        void Client(winrt::hstring const& v) { m_client = v; }

        winrt::hstring Progress() const { return m_progress; }
        void Progress(winrt::hstring const& v) { m_progress = v; }

        winrt::hstring DLSpeed() const { return m_dlSpeed; }
        void DLSpeed(winrt::hstring const& v) { m_dlSpeed = v; }

        winrt::hstring ULSpeed() const { return m_ulSpeed; }
        void ULSpeed(winrt::hstring const& v) { m_ulSpeed = v; }

        winrt::hstring Downloaded() const { return m_downloaded; }
        void Downloaded(winrt::hstring const& v) { m_downloaded = v; }

        winrt::hstring PeerStatus() const { return m_peerStatus; }
        void PeerStatus(winrt::hstring const& v) { m_peerStatus = v; }

        winrt::hstring Location() const { return m_location; }
        void Location(winrt::hstring const& v) { m_location = v; }

        winrt::hstring ConnectionTime() const { return m_connectionTime; }
        void ConnectionTime(winrt::hstring const& v) { m_connectionTime = v; }

        winrt::hstring Protocol() const { return m_protocol; }
        void Protocol(winrt::hstring const& v) { m_protocol = v; }

        winrt::hstring Initiator() const { return m_initiator; }
        void Initiator(winrt::hstring const& v) { m_initiator = v; }

        winrt::hstring Source() const { return m_source; }
        void Source(winrt::hstring const& v) { m_source = v; }

    private:
        winrt::hstring m_ip;
        winrt::hstring m_client;
        winrt::hstring m_progress;
        winrt::hstring m_dlSpeed;
        winrt::hstring m_ulSpeed;
        winrt::hstring m_downloaded;
        winrt::hstring m_peerStatus;
        winrt::hstring m_location;
        winrt::hstring m_connectionTime;
        winrt::hstring m_protocol;
        winrt::hstring m_initiator;
        winrt::hstring m_source;
    };

    // ---------------------------------------------------------------
    // TrackerDisplayItem
    // ---------------------------------------------------------------
    struct TrackerDisplayItem : TrackerDisplayItemT<TrackerDisplayItem>
    {
        TrackerDisplayItem() = default;

        winrt::hstring URL() const { return m_url; }
        void URL(winrt::hstring const& v) { m_url = v; }

        winrt::hstring Tier() const { return m_tier; }
        void Tier(winrt::hstring const& v) { m_tier = v; }

        winrt::hstring Peers() const { return m_peers; }
        void Peers(winrt::hstring const& v) { m_peers = v; }

        winrt::hstring Status() const { return m_status; }
        void Status(winrt::hstring const& v) { m_status = v; }

        winrt::hstring Message() const { return m_message; }
        void Message(winrt::hstring const& v) { m_message = v; }

    private:
        winrt::hstring m_url;
        winrt::hstring m_tier;
        winrt::hstring m_peers;
        winrt::hstring m_status;
        winrt::hstring m_message;
    };

    // ---------------------------------------------------------------
    // FileDisplayItem
    // ---------------------------------------------------------------
    struct FileDisplayItem : FileDisplayItemT<FileDisplayItem>
    {
        FileDisplayItem() = default;

        winrt::hstring Path() const { return m_path; }
        void Path(winrt::hstring const& v) { m_path = v; }

        winrt::hstring Size() const { return m_size; }
        void Size(winrt::hstring const& v) { m_size = v; }

        double ProgressValue() const { return m_progressValue; }
        void ProgressValue(double v) { m_progressValue = v; }

        winrt::hstring Done() const { return m_done; }
        void Done(winrt::hstring const& v) { m_done = v; }

        int32_t PriorityIndex() const { return m_priorityIndex; }
        void PriorityIndex(int32_t v) { m_priorityIndex = v; }

        int32_t FileIndex() const { return m_fileIndex; }
        void FileIndex(int32_t v) { m_fileIndex = v; }

    private:
        winrt::hstring m_path;
        winrt::hstring m_size;
        double m_progressValue{};
        winrt::hstring m_done;
        int32_t m_priorityIndex{};
        int32_t m_fileIndex{};
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct PeerDisplayItem : PeerDisplayItemT<PeerDisplayItem, implementation::PeerDisplayItem> {};
    struct TrackerDisplayItem : TrackerDisplayItemT<TrackerDisplayItem, implementation::TrackerDisplayItem> {};
    struct FileDisplayItem : FileDisplayItemT<FileDisplayItem, implementation::FileDisplayItem> {};
}
