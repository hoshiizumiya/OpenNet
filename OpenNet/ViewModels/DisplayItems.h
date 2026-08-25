#pragma once
#include "ViewModels/PeerDisplayItem.g.h"
#include "ViewModels/TrackerDisplayItem.g.h"
#include "ViewModels/TrackerLogDisplayItem.g.h"
#include "ViewModels/FileDisplayItem.g.h"
#include "ViewModels/RuntimeStatusDisplayItem.g.h"

import OpenNet.ViewModels.ObservableMixin;
import winrt.Microsoft.UI.Xaml.Data;

namespace winrt::OpenNet::ViewModels::implementation
{
	// ---------------------------------------------------------------
	// PeerDisplayItem
	// ---------------------------------------------------------------
	struct PeerDisplayItem : PeerDisplayItemT<PeerDisplayItem>,
		::OpenNet::ViewModels::ObservableMixin<PeerDisplayItem>
	{
		using ::OpenNet::ViewModels::ObservableMixin<PeerDisplayItem>::SetProperty;
		PeerDisplayItem()
		{
			m_children = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::PeerDisplayItem>();
		}

		winrt::hstring IP() const
		{
			return m_ip;
		}
		void IP(winrt::hstring const& v)
		{
			SetProperty(m_ip, v, L"IP");
		}

		winrt::hstring Address() const
		{
			return m_address;
		}
		void Address(winrt::hstring const& v)
		{
			SetProperty(m_address, v, L"Address");
		}

		std::int32_t Port() const
		{
			return m_port;
		}
		void Port(std::int32_t v)
		{
			SetProperty(m_port, v, L"Port");
		}

		winrt::hstring Client() const
		{
			return m_client;
		}
		void Client(winrt::hstring const& v)
		{
			SetProperty(m_client, v, L"Client");
		}

		winrt::hstring Progress() const
		{
			return m_progress;
		}
		void Progress(winrt::hstring const& v)
		{
			SetProperty(m_progress, v, L"Progress");
		}

		winrt::hstring DLSpeed() const
		{
			return m_dlSpeed;
		}
		void DLSpeed(winrt::hstring const& v)
		{
			SetProperty(m_dlSpeed, v, L"DLSpeed");
		}

		winrt::hstring ULSpeed() const
		{
			return m_ulSpeed;
		}
		void ULSpeed(winrt::hstring const& v)
		{
			SetProperty(m_ulSpeed, v, L"ULSpeed");
		}

		winrt::hstring Downloaded() const
		{
			return m_downloaded;
		}
		void Downloaded(winrt::hstring const& v)
		{
			SetProperty(m_downloaded, v, L"Downloaded");
		}

		winrt::hstring Uploaded() const
		{
			return m_uploaded;
		}
		void Uploaded(winrt::hstring const& v)
		{
			SetProperty(m_uploaded, v, L"Uploaded");
		}

		double ProgressValue() const
		{
			return m_progressValue;
		}
		void ProgressValue(double v)
		{
			SetProperty(m_progressValue, v, L"ProgressValue");
		}

		double SegmentStart() const
		{
			return m_segmentStart;
		}
		void SegmentStart(double value)
		{
			SetProperty(m_segmentStart, value, L"SegmentStart");
		}

		double SegmentEnd() const
		{
			return m_segmentEnd;
		}
		void SegmentEnd(double value)
		{
			SetProperty(m_segmentEnd, value, L"SegmentEnd");
		}

		winrt::Microsoft::UI::Xaml::Visibility LinearProgressVisibility() const
		{
			return m_linearProgressVisibility;
		}
		void LinearProgressVisibility(winrt::Microsoft::UI::Xaml::Visibility const value)
		{
			SetProperty(m_linearProgressVisibility, value, L"LinearProgressVisibility");
		}

		winrt::Microsoft::UI::Xaml::Visibility RangeProgressVisibility() const
		{
			return m_rangeProgressVisibility;
		}
		void RangeProgressVisibility(winrt::Microsoft::UI::Xaml::Visibility const value)
		{
			SetProperty(m_rangeProgressVisibility, value, L"RangeProgressVisibility");
		}
		std::int64_t DownloadRate() const
		{
			return m_downloadRate;
		}
		void DownloadRate(std::int64_t v)
		{
			SetProperty(m_downloadRate, v, L"DownloadRate");
		}

		std::int64_t UploadRate() const
		{
			return m_uploadRate;
		}
		void UploadRate(std::int64_t v)
		{
			SetProperty(m_uploadRate, v, L"UploadRate");
		}

		std::int64_t DownloadedBytes() const
		{
			return m_downloadedBytes;
		}
		void DownloadedBytes(std::int64_t v)
		{
			SetProperty(m_downloadedBytes, v, L"DownloadedBytes");
		}

		std::int64_t UploadedBytes() const
		{
			return m_uploadedBytes;
		}
		void UploadedBytes(std::int64_t v)
		{
			SetProperty(m_uploadedBytes, v, L"UploadedBytes");
		}

		winrt::hstring PeerStatus() const
		{
			return m_peerStatus;
		}
		void PeerStatus(winrt::hstring const& v)
		{
			SetProperty(m_peerStatus, v, L"PeerStatus");
		}

		winrt::hstring Reason() const
		{
			return m_reason;
		}
		void Reason(winrt::hstring const& v)
		{
			SetProperty(m_reason, v, L"Reason");
		}

		winrt::hstring Location() const
		{
			return m_location;
		}
		void Location(winrt::hstring const& v)
		{
			SetProperty(m_location, v, L"Location");
		}

		winrt::hstring CountryCode() const
		{
			return m_countryCode;
		}
		void CountryCode(winrt::hstring const& v)
		{
			SetProperty(m_countryCode, v, L"CountryCode");
		}

		winrt::hstring FlagSvg() const
		{
			return m_flagSvg;
		}
		void FlagSvg(winrt::hstring const& v)
		{
			SetProperty(m_flagSvg, v, L"FlagSvg");
		}

		winrt::hstring ConnectionTime() const
		{
			return m_connectionTime;
		}
		void ConnectionTime(winrt::hstring const& v)
		{
			SetProperty(m_connectionTime, v, L"ConnectionTime");
		}

		winrt::hstring Protocol() const
		{
			return m_protocol;
		}
		void Protocol(winrt::hstring const& v)
		{
			SetProperty(m_protocol, v, L"Protocol");
		}

		winrt::hstring Initiator() const
		{
			return m_initiator;
		}
		void Initiator(winrt::hstring const& v)
		{
			SetProperty(m_initiator, v, L"Initiator");
		}

		winrt::hstring Source() const
		{
			return m_source;
		}
		void Source(winrt::hstring const& v)
		{
			SetProperty(m_source, v, L"Source");
		}

		bool IsExpanded() const
		{
			return m_isExpanded;
		}
		void IsExpanded(bool value)
		{
			SetProperty(m_isExpanded, value, L"IsExpanded");
		}

		bool IsGroup() const
		{
			return m_isGroup;
		}
		void IsGroup(bool value)
		{
			SetProperty(m_isGroup, value, L"IsGroup");
		}

		winrt::Windows::Foundation::Collections::IObservableVector<
			winrt::OpenNet::ViewModels::PeerDisplayItem> Children() const
		{
			return m_children;
		}

	private:
		winrt::hstring m_ip;
		winrt::hstring m_address;
		std::int32_t m_port{};
		winrt::hstring m_client;
		winrt::hstring m_progress;
		winrt::hstring m_dlSpeed;
		winrt::hstring m_ulSpeed;
		winrt::hstring m_downloaded;
		winrt::hstring m_uploaded;
		double m_progressValue{};
		double m_segmentStart{};
		double m_segmentEnd{ 100.0 };
		winrt::Microsoft::UI::Xaml::Visibility m_linearProgressVisibility{
			winrt::Microsoft::UI::Xaml::Visibility::Collapsed };
		winrt::Microsoft::UI::Xaml::Visibility m_rangeProgressVisibility{
			winrt::Microsoft::UI::Xaml::Visibility::Collapsed };
		std::int64_t m_downloadRate{};
		std::int64_t m_uploadRate{};
		std::int64_t m_downloadedBytes{};
		std::int64_t m_uploadedBytes{};
		winrt::hstring m_peerStatus;
		winrt::hstring m_reason;
		winrt::hstring m_location;
		winrt::hstring m_countryCode;
		winrt::hstring m_flagSvg;
		winrt::hstring m_connectionTime;
		winrt::hstring m_protocol;
		winrt::hstring m_initiator;
		winrt::hstring m_source;
		bool m_isExpanded{ true };
		bool m_isGroup{ false };
		winrt::Windows::Foundation::Collections::IObservableVector<
			winrt::OpenNet::ViewModels::PeerDisplayItem> m_children{ nullptr };
	};

	// ---------------------------------------------------------------
	// TrackerDisplayItem
	// ---------------------------------------------------------------
	struct TrackerDisplayItem : TrackerDisplayItemT<TrackerDisplayItem>,
		::OpenNet::ViewModels::ObservableMixin<TrackerDisplayItem>
	{
		using ::OpenNet::ViewModels::ObservableMixin<TrackerDisplayItem>::SetProperty;
		TrackerDisplayItem() = default;

		winrt::hstring URL() const
		{
			return m_url;
		}
		void URL(winrt::hstring const& v)
		{
			SetProperty(m_url, v, L"URL");
		}

		winrt::hstring Tier() const
		{
			return m_tier;
		}
		void Tier(winrt::hstring const& v)
		{
			SetProperty(m_tier, v, L"Tier");
		}

		winrt::hstring Peers() const
		{
			return m_peers;
		}
		void Peers(winrt::hstring const& v)
		{
			SetProperty(m_peers, v, L"Peers");
		}

		winrt::hstring Retries() const
		{
			return m_retries;
		}
		void Retries(winrt::hstring const& v)
		{
			SetProperty(m_retries, v, L"Retries");
		}

		winrt::hstring TimeRemaining() const
		{
			return m_timeRemaining;
		}
		void TimeRemaining(winrt::hstring const& v)
		{
			SetProperty(m_timeRemaining, v, L"TimeRemaining");
		}

		winrt::hstring Seeders() const
		{
			return m_seeders;
		}
		void Seeders(winrt::hstring const& v)
		{
			SetProperty(m_seeders, v, L"Seeders");
		}

		winrt::hstring Leechers() const
		{
			return m_leechers;
		}
		void Leechers(winrt::hstring const& v)
		{
			SetProperty(m_leechers, v, L"Leechers");
		}

		winrt::hstring Downloaded() const
		{
			return m_downloaded;
		}
		void Downloaded(winrt::hstring const& v)
		{
			SetProperty(m_downloaded, v, L"Downloaded");
		}

		winrt::hstring Status() const
		{
			return m_status;
		}
		void Status(winrt::hstring const& v)
		{
			SetProperty(m_status, v, L"Status");
		}

		winrt::hstring Message() const
		{
			return m_message;
		}
		void Message(winrt::hstring const& v)
		{
			SetProperty(m_message, v, L"Message");
		}

		winrt::hstring ProgressPieces() const
		{
			return m_progressPieces;
		}
		void ProgressPieces(winrt::hstring const& value)
		{
			SetProperty(m_progressPieces, value, L"ProgressPieces");
		}

		winrt::hstring ProgressText() const
		{
			return m_progressText;
		}
		void ProgressText(winrt::hstring const& value)
		{
			SetProperty(m_progressText, value, L"ProgressText");
		}

		winrt::Microsoft::UI::Xaml::Visibility TrackerProgressVisibility() const
		{
			return m_trackerProgressVisibility;
		}
		void TrackerProgressVisibility(winrt::Microsoft::UI::Xaml::Visibility const value)
		{
			SetProperty(m_trackerProgressVisibility, value, L"TrackerProgressVisibility");
		}

		winrt::Microsoft::UI::Xaml::Visibility HttpProgressVisibility() const
		{
			return m_httpProgressVisibility;
		}
		void HttpProgressVisibility(winrt::Microsoft::UI::Xaml::Visibility const value)
		{
			SetProperty(m_httpProgressVisibility, value, L"HttpProgressVisibility");
		}
	private:
		winrt::hstring m_url;
		winrt::hstring m_tier;
		winrt::hstring m_peers;
		winrt::hstring m_retries;
		winrt::hstring m_timeRemaining;
		winrt::hstring m_seeders;
		winrt::hstring m_leechers;
		winrt::hstring m_downloaded;
		winrt::hstring m_status;
		winrt::hstring m_message;
		winrt::hstring m_progressPieces;
		winrt::hstring m_progressText;
		winrt::Microsoft::UI::Xaml::Visibility m_trackerProgressVisibility{
			winrt::Microsoft::UI::Xaml::Visibility::Visible };
		winrt::Microsoft::UI::Xaml::Visibility m_httpProgressVisibility{
			winrt::Microsoft::UI::Xaml::Visibility::Collapsed };
	};

	// ---------------------------------------------------------------
	// FileDisplayItem
	// ---------------------------------------------------------------
	struct FileDisplayItem : FileDisplayItemT<FileDisplayItem>,
		::OpenNet::ViewModels::ObservableMixin<FileDisplayItem>
	{
		using ::OpenNet::ViewModels::ObservableMixin<FileDisplayItem>::SetProperty;
		FileDisplayItem() = default;

		winrt::hstring Path() const
		{
			return m_path;
		}
		void Path(winrt::hstring const& v)
		{
			SetProperty(m_path, v, L"Path");
		}

		winrt::hstring Size() const
		{
			return m_size;
		}
		void Size(winrt::hstring const& v)
		{
			SetProperty(m_size, v, L"Size");
		}

		double ProgressValue() const
		{
			return m_progressValue;
		}
		void ProgressValue(double v)
		{
			SetProperty(m_progressValue, v, L"ProgressValue");
		}

		winrt::hstring Done() const
		{
			return m_done;
		}
		void Done(winrt::hstring const& v)
		{
			SetProperty(m_done, v, L"Done");
		}

		int32_t PriorityIndex() const
		{
			return m_priorityIndex;
		}
		void PriorityIndex(int32_t v)
		{
			SetProperty(m_priorityIndex, v, L"PriorityIndex");
		}

		int32_t FileIndex() const
		{
			return m_fileIndex;
		}
		void FileIndex(int32_t v)
		{
			SetProperty(m_fileIndex, v, L"FileIndex");
		}

	private:
		winrt::hstring m_path;
		winrt::hstring m_size;
		double m_progressValue{};
		winrt::hstring m_done;
		int32_t m_priorityIndex{};
		int32_t m_fileIndex{};
	};

	struct TrackerLogDisplayItem : TrackerLogDisplayItemT<TrackerLogDisplayItem>,
		::OpenNet::ViewModels::ObservableMixin<TrackerLogDisplayItem>
	{
		using ::OpenNet::ViewModels::ObservableMixin<TrackerLogDisplayItem>::SetProperty;
		TrackerLogDisplayItem() = default;

		winrt::hstring Time() const
		{
			return m_time;
		}
		void Time(winrt::hstring const& value)
		{
			SetProperty(m_time, value, L"Time");
		}
		winrt::hstring Content() const
		{
			return m_content;
		}
		void Content(winrt::hstring const& value)
		{
			SetProperty(m_content, value, L"Content");
		}
		bool IsError() const
		{
			return m_isError;
		}
		void IsError(bool const value)
		{
			SetProperty(m_isError, value, L"IsError");
		}

	private:
		winrt::hstring m_time;
		winrt::hstring m_content;
		bool m_isError{};
	};

	// ---------------------------------------------------------------
	// RuntimeStatusDisplayItem
	// ---------------------------------------------------------------
	struct RuntimeStatusDisplayItem :
		RuntimeStatusDisplayItemT<RuntimeStatusDisplayItem>,
		::OpenNet::ViewModels::ObservableMixin<RuntimeStatusDisplayItem>
	{
		using ::OpenNet::ViewModels::ObservableMixin<
			RuntimeStatusDisplayItem>::SetProperty;

		RuntimeStatusDisplayItem()
			: m_children(winrt::single_threaded_observable_vector<
						 winrt::OpenNet::ViewModels::RuntimeStatusDisplayItem>())
		{
		}

		winrt::hstring Name() const
		{
			return m_name;
		}
		void Name(winrt::hstring const& value)
		{
			SetProperty(m_name, value, L"Name");
		}

		winrt::hstring Value() const
		{
			return m_value;
		}
		void Value(winrt::hstring const& value)
		{
			SetProperty(m_value, value, L"Value");
		}

		bool IsExpanded() const
		{
			return m_isExpanded;
		}
		void IsExpanded(bool value)
		{
			SetProperty(m_isExpanded, value, L"IsExpanded");
		}

		bool IsGroup() const
		{
			return m_isGroup;
		}
		void IsGroup(bool value)
		{
			SetProperty(m_isGroup, value, L"IsGroup");
		}

		winrt::Windows::Foundation::Collections::IObservableVector<
			winrt::OpenNet::ViewModels::RuntimeStatusDisplayItem>
			Children() const
		{
			return m_children;
		}

	private:
		winrt::hstring m_name;
		winrt::hstring m_value;
		bool m_isExpanded{ true };
		bool m_isGroup{};
		winrt::Windows::Foundation::Collections::IObservableVector<
			winrt::OpenNet::ViewModels::RuntimeStatusDisplayItem>
			m_children{ nullptr };
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct PeerDisplayItem : PeerDisplayItemT<PeerDisplayItem, implementation::PeerDisplayItem>
	{
	};
	struct TrackerDisplayItem : TrackerDisplayItemT<TrackerDisplayItem, implementation::TrackerDisplayItem>
	{
	};
	struct FileDisplayItem : FileDisplayItemT<FileDisplayItem, implementation::FileDisplayItem>
	{
	};
	struct TrackerLogDisplayItem : TrackerLogDisplayItemT<
		TrackerLogDisplayItem, implementation::TrackerLogDisplayItem>
	{
	};
	struct RuntimeStatusDisplayItem :
		RuntimeStatusDisplayItemT<
		RuntimeStatusDisplayItem,
		implementation::RuntimeStatusDisplayItem>
	{
	};
}
