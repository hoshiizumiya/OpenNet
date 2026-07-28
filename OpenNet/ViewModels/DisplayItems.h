#pragma once
#include "ViewModels/PeerDisplayItem.g.h"
#include "ViewModels/TrackerDisplayItem.g.h"
#include "ViewModels/FileDisplayItem.g.h"

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
			m_children = winrt::single_threaded_observable_vector<
				winrt::OpenNet::ViewModels::PeerDisplayItem>();
		}

		winrt::hstring IP() const
		{
			return m_ip;
		}
		void IP(winrt::hstring const& v)
		{
			SetProperty(m_ip, v, L"IP");
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

		winrt::hstring PeerStatus() const
		{
			return m_peerStatus;
		}
		void PeerStatus(winrt::hstring const& v)
		{
			SetProperty(m_peerStatus, v, L"PeerStatus");
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
		winrt::hstring m_client;
		winrt::hstring m_progress;
		winrt::hstring m_dlSpeed;
		winrt::hstring m_ulSpeed;
		winrt::hstring m_downloaded;
		winrt::hstring m_peerStatus;
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
}
