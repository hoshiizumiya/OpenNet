#include "XamlWorkaround.h"
#include "TaskPeersListPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskPeersListPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskPeersListPage.g.cpp"
#endif
#include "Core/ClientFilter/ClientFilterManager.h"
#include "Core/IPFilter/IPFilterManager.h"
#include "ViewModels/DisplayItems.h"

import OpenNet.Core.P2PManager;
import OpenNet.Core.GeoIP.GeoIPManager;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ColumnWidthHelper;
import OpenNet.UI.Xaml.Control.DataTableColumnVisibilityHelper;
import OpenNet.UI.Xaml.Control.DataTableSortHelper;
import winrt.Microsoft.Windows.ApplicationModel.Resources;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation.Collections;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace ::OpenNet::Helpers;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskPeersListPage::TaskPeersListPage()
	{
		InitializeComponent();
		UpdateSortHeaders();
		this->NavigationCacheMode(winrt::Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Required);

		Loaded([this](auto, auto)
		{
			RestoreColumn(ColPeerIP(), "Peers.IP");
			RestoreColumn(ColPeerLocation(), "Peers.Location");
			RestoreColumn(ColPeerProgress(), "Peers.Progress");
			RestoreColumn(ColPeerDLSpeed(), "Peers.DLSpeed");
			RestoreColumn(ColPeerULSpeed(), "Peers.ULSpeed");
			RestoreColumn(ColPeerDownloaded(), "Peers.Downloaded");
			RestoreColumn(ColPeerUploaded(), "Peers.Uploaded");
			RestoreColumn(ColPeerClient(), "Peers.Client");
			RestoreColumn(ColPeerStatus(), "Peers.Status");
			RestoreColumn(ColPeerReason(), "Peers.Reason");
			RestoreColumn(ColPeerProtocol(), "Peers.Protocol");
			RestoreColumn(ColPeerInitiator(), "Peers.Initiator");
			RestoreColumn(ColPeerSource(), "Peers.Source");
			ScheduleRowLayoutSynchronization();
		});
		Unloaded([this](auto, auto)
		{
			SaveColumnWidth("Peers.IP", ColPeerIP());
			SaveColumnWidth("Peers.Location", ColPeerLocation());
			SaveColumnWidth("Peers.Progress", ColPeerProgress());
			SaveColumnWidth("Peers.DLSpeed", ColPeerDLSpeed());
			SaveColumnWidth("Peers.ULSpeed", ColPeerULSpeed());
			SaveColumnWidth("Peers.Downloaded", ColPeerDownloaded());
			SaveColumnWidth("Peers.Uploaded", ColPeerUploaded());
			SaveColumnWidth("Peers.Client", ColPeerClient());
			SaveColumnWidth("Peers.Status", ColPeerStatus());
			SaveColumnWidth("Peers.Reason", ColPeerReason());
			SaveColumnWidth("Peers.Protocol", ColPeerProtocol());
			SaveColumnWidth("Peers.Initiator", ColPeerInitiator());
			SaveColumnWidth("Peers.Source", ColPeerSource());
		});

		// DataColumn resizing does not change the DataTable's outer size, so a
		// virtualized TreeView row can miss the layout invalidation until its data
		// changes. Observe each header column and coalesce one realized-row pass.
		auto weak = get_weak();
		for (auto const& column : std::array{
			ColPeerIP(), ColPeerLocation(), ColPeerProgress(), ColPeerDLSpeed(),
			ColPeerULSpeed(), ColPeerDownloaded(), ColPeerUploaded(), ColPeerClient(),
			ColPeerStatus(), ColPeerReason(), ColPeerProtocol(), ColPeerInitiator(),
			ColPeerSource() })
		{
			column.SizeChanged([weak](auto const&, auto const&)
			{
				if (auto self = weak.get())
					self->ScheduleRowLayoutSynchronization();
			});
		}
	}

	TaskPeersListPage::~TaskPeersListPage()
	{
		Unsubscribe();
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
			m_refreshTimer.Tick(m_timerTickToken);
			m_refreshTimer = nullptr;
		}
	}

	void TaskPeersListPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		Unsubscribe();

		m_viewModel = e.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
		{
			m_viewModel = this->DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		}

		if (m_viewModel)
		{
			this->DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged(
				{ this, &TaskPeersListPage::OnViewModelPropertyChanged });
		}

		// Set up periodic refresh timer (every 2 seconds)
		if (!m_refreshTimer)
		{
			m_refreshTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
			m_timerTickToken = m_refreshTimer.Tick(
				{ this, &TaskPeersListPage::OnRefreshTimerTick });
		}
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		m_refreshTimer.Interval(std::chrono::milliseconds(
			std::clamp<std::int64_t>(
				database.GetInt("ui", "refresh_interval_ms").value_or(1000),
				100,
				60000)));
		m_refreshTimer.Start();

		RefreshPeerList();
	}

	void TaskPeersListPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
		}
		Unsubscribe();
	}

	void TaskPeersListPage::Unsubscribe()
	{
		if (m_viewModel && m_vmPropertyChangedToken.value)
		{
			m_viewModel.PropertyChanged(m_vmPropertyChangedToken);
			m_vmPropertyChangedToken = {};
		}
		m_viewModel = nullptr;
	}

	void TaskPeersListPage::OnViewModelPropertyChanged(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
		{
			// Task changed — force full rebuild
			m_lastTaskId.clear();
			m_peerItems = nullptr;
			RefreshPeerList();
		}
	}

	void TaskPeersListPage::OnRefreshTimerTick(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Windows::Foundation::IInspectable const&)
	{
		RefreshPeerList();
	}

	void TaskPeersListPage::ColumnHeader_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto button = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::Button>();
		if (!button || !button.Tag()) return;
		auto const column = winrt::unbox_value<winrt::hstring>(button.Tag());
		if (m_sortColumn == column)
			m_sortDirection = (m_sortDirection + 1) % 3;
		else
		{
			m_sortColumn = column;
			m_sortDirection = 1;
		}
		UpdateSortHeaders();
		RefreshPeerList();
	}

	void TaskPeersListPage::UpdateSortHeaders()
	{
		auto update = [this](auto const& button)
		{
			::OpenNet::UI::Xaml::Control::DataTableSortHelper::UpdateHeader(
				button, m_sortColumn, m_sortDirection);
		};
		update(SortPeerIpButton());
		update(SortPeerLocationButton());
		update(SortPeerProgressButton());
		update(SortPeerDlSpeedButton());
		update(SortPeerUlSpeedButton());
		update(SortPeerDownloadedButton());
		update(SortPeerUploadedButton());
		update(SortPeerClientButton());
		update(SortPeerStatusButton());
		update(SortPeerReasonButton());
		update(SortPeerProtocolButton());
		update(SortPeerInitiatorButton());
		update(SortPeerSourceButton());
	}

	void TaskPeersListPage::SortPeerItems(
		std::vector<winrt::OpenNet::ViewModels::PeerDisplayItem>& items)
	{
		if (m_sortDirection == 0) return;
		auto const column = m_sortColumn;
		auto const direction = m_sortDirection;
		auto compareText = [](winrt::hstring const& left, winrt::hstring const& right)
		{
			return _wcsicmp(left.c_str(), right.c_str());
		};
		auto compare = [column, compareText](auto const& left, auto const& right)
		{
			const auto compareNumber = [](auto lhs, auto rhs)
			{
				return lhs < rhs ? -1 : lhs > rhs ? 1 : 0;
			};
			if (column == L"Progress")
				return compareNumber(left.ProgressValue(), right.ProgressValue());
			if (column == L"DLSpeed")
				return compareNumber(left.DownloadRate(), right.DownloadRate());
			if (column == L"ULSpeed")
				return compareNumber(left.UploadRate(), right.UploadRate());
			if (column == L"Downloaded")
				return compareNumber(left.DownloadedBytes(), right.DownloadedBytes());
			if (column == L"Uploaded")
				return compareNumber(left.UploadedBytes(), right.UploadedBytes());
			if (column == L"IP") return compareText(left.IP(), right.IP());
			if (column == L"Location") return compareText(left.Location(), right.Location());
			if (column == L"Client") return compareText(left.Client(), right.Client());
			if (column == L"Status") return compareText(left.PeerStatus(), right.PeerStatus());
			if (column == L"Reason") return compareText(left.Reason(), right.Reason());
			if (column == L"Protocol") return compareText(left.Protocol(), right.Protocol());
			if (column == L"Initiator") return compareText(left.Initiator(), right.Initiator());
			return compareText(left.Source(), right.Source());
		};
		std::stable_sort(items.begin(), items.end(),
						 [direction, compare](auto const& left, auto const& right)
		{
			return direction == 1
				? compare(left, right) < 0
				: compare(left, right) > 0;
		});
	}

	void TaskPeersListPage::ColumnHeader_PointerPressed(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
	{
		if (!args.GetCurrentPoint(nullptr).Properties().IsRightButtonPressed())
			return;
		auto source = args.OriginalSource().try_as<DependencyObject>();
		while (source)
		{
			if (auto column = source.try_as<winrt::XamlToolkit::Labs::WinUI::DataColumn>())
			{
				m_contextColumn = column;
				return;
			}
			source = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(source);
		}
	}

	void TaskPeersListPage::ColumnHeader_RightTapped(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
	{
		m_contextColumn = nullptr;
		auto source = args.OriginalSource().try_as<DependencyObject>();
		while (source)
		{
			if (auto column = source.try_as<winrt::XamlToolkit::Labs::WinUI::DataColumn>())
			{
				m_contextColumn = column;
				break;
			}
			source = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(source);
		}
	}

	void TaskPeersListPage::ColumnMenu_Opening(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Windows::Foundation::IInspectable const&)
	{
		AutoSizeSelectedColumnItem().IsEnabled(m_contextColumn != nullptr);
		if (auto menu = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::MenuFlyout>())
		{
			for (auto const& entry : menu.Items())
			{
				if (auto toggle = entry.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>())
				{
					auto column = ColumnForTag(
						winrt::unbox_value<winrt::hstring>(toggle.Tag()));
					toggle.IsChecked(column && column.Visibility() == Visibility::Visible);
				}
			}
		}
	}

	winrt::XamlToolkit::Labs::WinUI::DataColumn TaskPeersListPage::ColumnForTag(
		winrt::hstring const& tag)
	{
		if (tag == L"IP") return ColPeerIP();
		if (tag == L"Location") return ColPeerLocation();
		if (tag == L"Progress") return ColPeerProgress();
		if (tag == L"DLSpeed") return ColPeerDLSpeed();
		if (tag == L"ULSpeed") return ColPeerULSpeed();
		if (tag == L"Downloaded") return ColPeerDownloaded();
		if (tag == L"Uploaded") return ColPeerUploaded();
		if (tag == L"Client") return ColPeerClient();
		if (tag == L"Status") return ColPeerStatus();
		if (tag == L"Reason") return ColPeerReason();
		if (tag == L"Protocol") return ColPeerProtocol();
		if (tag == L"Initiator") return ColPeerInitiator();
		if (tag == L"Source") return ColPeerSource();
		return nullptr;
	}

	void TaskPeersListPage::ColumnVisibility_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto toggle = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>();
		if (!toggle || !toggle.Tag()) return;
		if (auto column = ColumnForTag(winrt::unbox_value<winrt::hstring>(toggle.Tag())))
		{
			column.Visibility(toggle.IsChecked() ? Visibility::Visible : Visibility::Collapsed);
			SynchronizePeerRows();
		}
	}

	void TaskPeersListPage::AutoSizeColumn(
		winrt::XamlToolkit::Labs::WinUI::DataColumn const& column)
	{
		if (!column) return;
		column.DesiredWidth(GridLengthHelper::Auto());
		column.InvalidateMeasure();
		PeersTreeView().InvalidateMeasure();
	}

	void TaskPeersListPage::AutoSizeSelectedColumn_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		AutoSizeColumn(m_contextColumn);
	}

	void TaskPeersListPage::AutoSizeAllColumns_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		for (auto const& column : std::array{
			ColPeerIP(), ColPeerLocation(), ColPeerProgress(), ColPeerDLSpeed(),
			ColPeerULSpeed(), ColPeerDownloaded(), ColPeerUploaded(), ColPeerClient(), ColPeerStatus(),
			ColPeerReason(), ColPeerProtocol(), ColPeerInitiator(), ColPeerSource() })
			AutoSizeColumn(column);
	}

	void TaskPeersListPage::ResetColumns_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
	{
		m_sortColumn = {};
		m_sortDirection = 0;
		UpdateSortHeaders();
		for (auto const& column : std::array{
			ColPeerIP(), ColPeerLocation(), ColPeerProgress(), ColPeerDLSpeed(),
			ColPeerULSpeed(), ColPeerDownloaded(), ColPeerUploaded(), ColPeerClient(), ColPeerStatus(),
			ColPeerReason(), ColPeerProtocol(), ColPeerInitiator(), ColPeerSource() })
			column.Visibility(Visibility::Visible);
		SynchronizePeerRows();
		AutoSizeAllColumns_Click(sender, args);
		RefreshPeerList();
	}

	void TaskPeersListPage::PeerDataRow_Loaded(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		std::array const columns{
			ColPeerIP(), ColPeerLocation(), ColPeerProgress(), ColPeerDLSpeed(),
			ColPeerULSpeed(), ColPeerDownloaded(), ColPeerUploaded(), ColPeerClient(), ColPeerStatus(),
			ColPeerReason(), ColPeerProtocol(), ColPeerInitiator(), ColPeerSource() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRow(
			sender.try_as<winrt::XamlToolkit::Labs::WinUI::DataRow>(),
			columns.data(), static_cast<unsigned int>(columns.size()));
	}

	void TaskPeersListPage::SynchronizePeerRows()
	{
		std::array const columns{
			ColPeerIP(), ColPeerLocation(), ColPeerProgress(), ColPeerDLSpeed(),
			ColPeerULSpeed(), ColPeerDownloaded(), ColPeerUploaded(), ColPeerClient(), ColPeerStatus(),
			ColPeerReason(), ColPeerProtocol(), ColPeerInitiator(), ColPeerSource() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRealizedRows(
			PeersTreeView(), columns.data(), static_cast<unsigned int>(columns.size()));
		PeersTreeView().InvalidateMeasure();
	}

	void TaskPeersListPage::ScheduleRowLayoutSynchronization()
	{
		if (m_rowLayoutSynchronizationQueued)
			return;
		m_rowLayoutSynchronizationQueued = true;
		auto weak = get_weak();
		if (!DispatcherQueue().TryEnqueue([weak]()
			{
				if (auto self = weak.get())
				{
					self->m_rowLayoutSynchronizationQueued = false;
					self->SynchronizePeerRows();
				}
			}))
		{
			m_rowLayoutSynchronizationQueued = false;
			SynchronizePeerRows();
		}
	}

	void TaskPeersListPage::ResetPeerGroups()
	{
		m_peerItems = nullptr;
		m_connectedGroup = nullptr;
		m_connectingGroup = nullptr;
		m_disconnectingGroup = nullptr;
		m_banIpGroup = nullptr;
		m_lastActivePeers.clear();
		m_disconnectingPeers.clear();
		m_banIpPeers.clear();
		if (auto tree = PeersTreeView())
			tree.ItemsSource(nullptr);
	}

	void TaskPeersListPage::EnsurePeerGroups()
	{
		if (m_peerItems)
			return;

		m_peerItems = winrt::single_threaded_observable_vector<
			winrt::Windows::Foundation::IInspectable>();

		auto makeGroup = [](winrt::hstring const& title)
		{
			auto group = winrt::make<
				winrt::OpenNet::ViewModels::implementation::PeerDisplayItem>();
			group.IP(title);
			group.FlagSvg(
				L"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1 1\"/>");
			group.IsGroup(true);
			group.IsExpanded(true);
			return group;
		};

		m_connectedGroup = makeGroup(L"bt_connected");
		m_connectingGroup = makeGroup(L"bt_connecting");
		m_disconnectingGroup = makeGroup(L"disconnecting");
		m_banIpGroup = makeGroup(L"BanIP");
		m_peerItems.Append(m_connectedGroup);
		m_peerItems.Append(m_connectingGroup);
		m_peerItems.Append(m_disconnectingGroup);
		m_peerItems.Append(m_banIpGroup);
		PeersTreeView().ItemsSource(m_peerItems);
	}

	winrt::hstring TaskPeersListPage::BuildFlagSvg(std::string countryCode)
	{
		static winrt::hstring const emptyFlag{
			L"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1 1\"/>" };
		std::transform(countryCode.begin(), countryCode.end(), countryCode.begin(),
					   [](unsigned char value)
		{
			return static_cast<char>(std::tolower(value));
		});
		if (countryCode.size() != 2)
			return emptyFlag;

		if (auto const cached = m_flagSvgCache.find(countryCode);
			cached != m_flagSvgCache.end())
		{
			return cached->second;
		}

		if (m_flagSprite.empty())
		{
			wchar_t executablePath[MAX_PATH]{};
			auto const length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
			if (length > 0 && length < MAX_PATH)
			{
				auto const spritePath =
					std::filesystem::path(executablePath).parent_path() /
					L"Assets" / L"IP2Location" / L"sprite.svg";
				std::ifstream stream(spritePath, std::ios::binary);
				if (stream)
				{
					m_flagSprite.assign(
						std::istreambuf_iterator<char>(stream),
						std::istreambuf_iterator<char>());
				}
			}
		}

		auto const symbolStart = m_flagSprite.find(
			"<symbol id=\"" + countryCode + "\"");
		if (symbolStart == std::string::npos)
			return emptyFlag;
		auto const contentStart = m_flagSprite.find('>', symbolStart);
		auto const symbolEnd = m_flagSprite.find("</symbol>", contentStart);
		if (contentStart == std::string::npos || symbolEnd == std::string::npos)
			return emptyFlag;

		auto const openingTag =
			m_flagSprite.substr(symbolStart, contentStart - symbolStart + 1);
		std::string viewBox = "0 0 640 480";
		auto const viewBoxStart = openingTag.find("viewBox=\"");
		if (viewBoxStart != std::string::npos)
		{
			auto const valueStart = viewBoxStart + 9;
			auto const valueEnd = openingTag.find('"', valueStart);
			if (valueEnd != std::string::npos)
				viewBox = openingTag.substr(valueStart, valueEnd - valueStart);
		}

		std::string svg =
			"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" +
			viewBox + "\">" +
			m_flagSprite.substr(contentStart + 1, symbolEnd - contentStart - 1) +
			"</svg>";
		auto result = winrt::to_hstring(svg);
		m_flagSvgCache.emplace(countryCode, result);
		return result;
	}

	// Helper to format speed
	static winrt::hstring FormatSpeed(int kbps)
	{
		if (kbps >= 1024)
		{
			wchar_t buf[64];
			swprintf(buf, 64, L"%.1f MB/s", kbps / 1024.0);
			return winrt::hstring{ buf };
		}
		wchar_t buf[64];
		swprintf(buf, 64, L"%d KB/s", kbps);
		return winrt::hstring{ buf };
	}

	static winrt::hstring FormatBytes(int64_t bytes)
	{
		if (bytes <= 0) return L"-";
		wchar_t buf[64];
		double gb = bytes / (1024.0 * 1024.0 * 1024.0);
		if (gb >= 1.0)
		{
			swprintf(buf, 64, L"%.2f GB", gb);
		}
		else
		{
			double mb = bytes / (1024.0 * 1024.0);
			if (mb >= 1.0)
				swprintf(buf, 64, L"%.2f MB", mb);
			else
				swprintf(buf, 64, L"%.1f KB", bytes / 1024.0);
		}
		return winrt::hstring{ buf };
	}

	// Format peer status flags to qBittorrent-style string: D=downloading, U=uploading, etc.
	static winrt::hstring FormatPeerStatus(uint32_t flags)
	{
		std::wstring result;
		// lt::peer_info flag bits (libtorrent 2.0)
		constexpr uint32_t interesting = 0x1;
		constexpr uint32_t choked = 0x2;
		constexpr uint32_t remote_interested = 0x4;
		constexpr uint32_t remote_choked = 0x8;
		constexpr uint32_t seed = 0x200;
		constexpr uint32_t optimistic_unchoke = 0x800;
		constexpr uint32_t snubbed = 0x1000;
		constexpr uint32_t rc4_encrypted = 0x100000;
		constexpr uint32_t plaintext_encrypted = 0x200000;

		if (flags & interesting) result += L"D";        // we want data from them
		if (flags & remote_interested) result += L"U";  // they want data from us
		if (flags & choked) result += L"d";              // we are choked by them
		if (flags & remote_choked) result += L"u";       // we are choking them
		if (flags & seed) result += L"S";                // seed
		if (flags & optimistic_unchoke) result += L"O";  // optimistic unchoke
		if (flags & snubbed) result += L"H";             // snubbed
		if (flags & rc4_encrypted) result += L"E";       // encrypted (RC4)
		else if (flags & plaintext_encrypted) result += L"e"; // encrypted (plaintext)

		return winrt::hstring{ result };
	}

	static winrt::hstring FormatConnectionType(int connType)
	{
		// 0_bit = 1, 1_bit = 2, 2_bit = 4 in libtorrent bitfield flags
		switch (connType)
		{
			case 1: return L"BT";         // standard_bittorrent = 0_bit -> value 1
			case 2: return L"WebSeed";    // web_seed = 1_bit -> value 2
			case 4: return L"HTTP";       // http_seed = 2_bit -> value 4
			default: return L"BT";
		}
	}

	static winrt::hstring FormatPeerSource(int source)
	{
		std::wstring parts;
		if (source & 1) parts += L"Tracker ";   // tracker = 0_bit -> 1
		if (source & 2) parts += L"DHT ";       // dht = 1_bit -> 2
		if (source & 4) parts += L"PEX ";       // pex = 2_bit -> 4
		if (source & 8) parts += L"LSD ";       // lsd = 3_bit -> 8
		if (source & 16) parts += L"Resume ";   // resume_data = 4_bit -> 16
		if (source & 32) parts += L"Incoming ";  // incoming = 5_bit -> 32
		// Trim trailing space
		if (!parts.empty() && parts.back() == L' ')
			parts.pop_back();
		return parts.empty() ? L"-" : winrt::hstring{ parts };
	}

	static std::string PeerEndpointKey(std::string const& ip, int port)
	{
		return ip + '\n' + std::to_string(port);
	}

	static winrt::hstring PeerEndpointText(std::string const& ip, int port)
	{
		if (ip.find(':') != std::string::npos)
		{
			return L"[" + winrt::to_hstring(ip) + L"]:"
				+ winrt::to_hstring(port);
		}
		return winrt::to_hstring(ip) + L":" + winrt::to_hstring(port);
	}

	static winrt::hstring PeerResource(
		wchar_t const* key, wchar_t const* fallback)
	{
		try
		{
			auto value =
				winrt::Microsoft::Windows::ApplicationModel::Resources::
				ResourceLoader{}.GetString(key);
			if (!value.empty())
				return value;
		}
		catch (...)
		{
		}
		return fallback;
	}

	static winrt::hstring FormatManualBanReason(
		::OpenNet::Core::IPBanEntry const& ban,
		std::int64_t now)
	{
		std::wstring text{
			PeerResource(L"PeerReason_Manual", L"User manual ban").c_str() };
		text += L" \u00b7 ";
		if (ban.expiresAt == 0)
		{
			text += PeerResource(
				L"PeerReason_Permanent", L"Permanent").c_str();
			return winrt::hstring{ text };
		}

		auto const remaining = std::max<std::int64_t>(0, ban.expiresAt - now);
		text += PeerResource(
			L"PeerReason_Remaining", L"Remaining").c_str();
		text += L" ";
		if (remaining < 60)
		{
			text += L"<1m";
		}
		else
		{
			auto const totalMinutes = (remaining + 59) / 60;
			auto const hours = totalMinutes / 60;
			auto const minutes = totalMinutes % 60;
			if (hours > 0)
				text += std::to_wstring(hours) + L"h";
			if (hours > 0 && minutes > 0)
				text += L" ";
			if (minutes > 0 || hours == 0)
				text += std::to_wstring(minutes) + L"m";
		}
		return winrt::hstring{ text };
	}

	void TaskPeersListPage::RefreshPeerList()
	{
		auto listView = PeersTreeView();
		auto emptyText = EmptyStateText();
		if (!listView) return;

		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			listView.ItemsSource(nullptr);
			ResetPeerGroups();
			m_lastTaskId.clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto selectedTask = m_viewModel.SelectedTask();
		auto taskType = selectedTask.TaskType();

		// Only show peers for BitTorrent tasks
		if (taskType != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			listView.ItemsSource(nullptr);
			ResetPeerGroups();
			m_lastTaskId.clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto taskId = winrt::to_string(selectedTask.TaskId());
		if (taskId.empty())
		{
			listView.ItemsSource(nullptr);
			ResetPeerGroups();
			m_lastTaskId.clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		// If task changed, clear cached items
		if (taskId != m_lastTaskId)
		{
			ResetPeerGroups();
			m_lastTaskId = taskId;
		}

		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
		{
			listView.ItemsSource(nullptr);
			ResetPeerGroups();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto detail = p2p.TorrentCore()->GetTorrentDetail(taskId);

		EnsurePeerGroups();

		auto initializeEndpointItem =
			[this](std::string const& ip, int port, auto const& item)
		{
			item.Address(winrt::to_hstring(ip));
			item.Port(port);
			item.IP(PeerEndpointText(ip, port));
			auto& geo = ::OpenNet::Core::GeoIPManager::Instance();
			auto const countryCode = geo.LookupCountryCode(ip);
			auto const country = geo.LookupCountryName(ip);
			item.CountryCode(winrt::to_hstring(countryCode));
			item.FlagSvg(BuildFlagSvg(countryCode));
			item.Location(country.empty() ? L"-" : winrt::to_hstring(country));
			item.IsGroup(false);
		};
		auto updateItem =
			[&initializeEndpointItem](auto const& peer, auto const& item)
		{
			initializeEndpointItem(peer.ip, peer.port, item);
			item.Client(winrt::to_hstring(peer.client));
			wchar_t progress[32];
			swprintf(progress, 32, L"%.1f%%", peer.progress * 100.0);
			item.Progress(progress);
			item.DLSpeed(FormatSpeed(peer.downloadRateKB));
			item.ULSpeed(FormatSpeed(peer.uploadRateKB));
			item.Downloaded(FormatBytes(peer.totalDownloaded));
			item.Uploaded(FormatBytes(peer.totalUploaded));
			item.ProgressValue(peer.progress);
			item.DownloadRate(peer.downloadRateKB);
			item.UploadRate(peer.uploadRateKB);
			item.DownloadedBytes(peer.totalDownloaded);
			item.UploadedBytes(peer.totalUploaded);
			item.PeerStatus(FormatPeerStatus(peer.flags));
			item.Reason(L"");
			item.ConnectionTime(L"-");
			item.Protocol(FormatConnectionType(peer.connectionType));
			item.Initiator(peer.isIncoming ? L"Remote" : L"Local");
			item.Source(FormatPeerSource(peer.source));
		};
		auto makeEndpointItem =
			[&initializeEndpointItem](std::string const& ip, int port)
		{
			auto item = winrt::make<
				winrt::OpenNet::ViewModels::implementation::PeerDisplayItem>();
			initializeEndpointItem(ip, port, item);
			item.Client(L"-");
			item.Progress(L"-");
			item.DLSpeed(L"-");
			item.ULSpeed(L"-");
			item.Downloaded(L"-");
			item.Uploaded(L"-");
			item.ConnectionTime(L"-");
			item.Protocol(L"-");
			item.Initiator(L"-");
			item.Source(L"-");
			return item;
		};

		auto& ipFilter = ::OpenNet::Core::IPFilterManager::Instance();
		auto const now = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		auto const activeBans = ipFilter.GetActiveBans(taskId);
		std::unordered_map<std::string, ::OpenNet::Core::IPBanEntry> bansByIp;
		for (auto const& ban : activeBans)
			bansByIp.emplace(ban.ip, ban);

		std::unordered_map<std::string, ::OpenNet::Core::ClientFilterHit>
			clientHitsByIp;
		auto& clientFilter =
			::OpenNet::Core::ClientFilterManager::Instance();
		for (auto const& hit :
			 clientFilter.GetRecentHits(500))
		{
			clientHitsByIp.emplace(hit.ip, hit);
		}

		auto previousActive = m_lastActivePeers;
		auto previousBanPeers = m_banIpPeers;
		// Rebuild this group from current ban sources on every refresh. This
		// removes expired/disabled rules without leaving stale BanIP rows.
		m_banIpPeers.clear();
		std::unordered_map<std::string, winrt::OpenNet::ViewModels::PeerDisplayItem>
			currentPeers;
		std::vector<winrt::OpenNet::ViewModels::PeerDisplayItem> connected;
		std::vector<winrt::OpenNet::ViewModels::PeerDisplayItem> connecting;
		for (auto const& peer : detail.peers)
		{
			auto const key = PeerEndpointKey(peer.ip, peer.port);
			auto item = previousActive.contains(key)
				? previousActive.at(key)
				: winrt::make<
				winrt::OpenNet::ViewModels::implementation::PeerDisplayItem>();
			updateItem(peer, item);
			currentPeers.emplace(key, item);
			m_disconnectingPeers.erase(key);
			m_banIpPeers.erase(key);

			if (!bansByIp.contains(peer.ip))
			{
				if (peer.isConnecting)
					connecting.push_back(item);
				else
					connected.push_back(item);
			}
		}

		for (auto const& [key, item] : previousActive)
		{
			if (!currentPeers.contains(key))
			{
				item.PeerStatus(L"disconnecting");
				item.Reason(L"connection_closed");
				m_disconnectingPeers[key] = item;
			}
		}

		auto peerEvents =
			p2p.TorrentCore()->GetRecentPeerEvents(taskId);
		std::unordered_map<
			std::string, std::optional<::OpenNet::Core::IPRule>>
			staticRuleMatches;
		if (ipFilter.IsEnabled())
		{
			std::vector<std::string> addresses;
			for (auto const& event : peerEvents)
			{
				if (event.reason == "ip_filter"
					&& !staticRuleMatches.contains(event.ip))
				{
					staticRuleMatches.emplace(event.ip, std::nullopt);
					addresses.push_back(event.ip);
				}
			}
			auto matches = ipFilter.FindMatchingRules(addresses);
			for (std::size_t index = 0; index < addresses.size(); ++index)
				staticRuleMatches[addresses[index]] = std::move(matches[index]);
		}
		for (auto const& event : peerEvents)
		{
			auto const key = PeerEndpointKey(event.ip, event.port);
			// The endpoint has since reconnected. Its current snapshot is more
			// authoritative than an older terminal event.
			if (currentPeers.contains(key) && !bansByIp.contains(event.ip))
				continue;

			auto item = previousActive.contains(key)
				? previousActive.at(key)
				: m_disconnectingPeers.contains(key)
				? m_disconnectingPeers.at(key)
				: previousBanPeers.contains(key)
				? previousBanPeers.at(key)
				: makeEndpointItem(event.ip, event.port);
			initializeEndpointItem(event.ip, event.port, item);

			auto const clientHit = clientHitsByIp.find(event.ip);
			auto const activeClientRule =
				clientFilter.IsEnabled()
				&& clientHit != clientHitsByIp.end()
				? clientFilter.MatchClient(clientHit->second.client)
				: std::optional<::OpenNet::Core::ClientFilterRule>{};
			auto const hasClientRule = activeClientRule.has_value();
			auto const ruleMatch = staticRuleMatches.find(event.ip);
			auto const matchingRule =
				ruleMatch == staticRuleMatches.end()
				? std::optional<::OpenNet::Core::IPRule>{}
			: ruleMatch->second;
			auto const remainsBanned =
				event.reason == "anti_leech"
				|| bansByIp.contains(event.ip)
				|| hasClientRule
				|| matchingRule.has_value();

			if (event.isBan && remainsBanned)
			{
				item.PeerStatus(L"BanIP");
				if (event.reason == "anti_leech")
				{
					item.Reason(PeerResource(
						L"PeerReason_AntiLeech", L"Anti-leech ban"));
				}
				else if (hasClientRule)
				{
					std::wstring reason{
						PeerResource(
							L"PeerReason_ClientListMatch",
							L"Anti-leech client list match").c_str() };
					if (!activeClientRule->pattern.empty())
					{
						reason += L": ";
						reason += winrt::to_hstring(
							activeClientRule->pattern).c_str();
					}
					item.Reason(winrt::hstring{ reason });
				}
				else if (matchingRule)
				{
					std::wstring reason{
						PeerResource(
							L"PeerReason_ListMatch",
							L"IP list source match").c_str() };
					if (!matchingRule->description.empty())
					{
						reason += L": ";
						reason += winrt::to_hstring(
							matchingRule->description).c_str();
					}
					item.Reason(winrt::hstring{ reason });
				}
				else
				{
					item.Reason(PeerResource(
						L"PeerReason_IPFilter", L"IP filter match"));
				}
				m_disconnectingPeers.erase(key);
				m_banIpPeers[key] = item;
			}
			else
			{
				item.PeerStatus(L"disconnecting");
				item.Reason(winrt::to_hstring(
					event.isBan ? "ip_filter_released" : event.reason));
				m_banIpPeers.erase(key);
				m_disconnectingPeers[key] = item;
			}
		}

		// Persistent manual bans are processed last so a subsequent generic
		// disconnect alert cannot hide their source or remaining duration.
		for (auto const& ban : activeBans)
		{
			auto const key = PeerEndpointKey(ban.ip, ban.port);
			auto item = currentPeers.contains(key)
				? currentPeers.at(key)
				: previousActive.contains(key)
				? previousActive.at(key)
				: m_banIpPeers.contains(key)
				? m_banIpPeers.at(key)
				: previousBanPeers.contains(key)
				? previousBanPeers.at(key)
				: makeEndpointItem(ban.ip, ban.port);
			initializeEndpointItem(ban.ip, ban.port, item);
			if (!ban.client.empty())
				item.Client(winrt::to_hstring(ban.client));
			item.PeerStatus(L"BanIP");
			item.Reason(FormatManualBanReason(ban, now));
			m_disconnectingPeers.erase(key);
			m_banIpPeers[key] = item;
		}
		m_lastActivePeers = std::move(currentPeers);

		auto replaceChildren = [](auto const& target, auto const& values)
		{
			for (std::uint32_t index = 0;
				 index < static_cast<std::uint32_t>(values.size());
				 ++index)
			{
				if (index < target.Size() && target.GetAt(index) == values[index])
				{
					continue;
				}

				std::uint32_t sourceIndex = index;
				while (sourceIndex < target.Size() &&
					   target.GetAt(sourceIndex) != values[index])
				{
					++sourceIndex;
				}
				if (sourceIndex < target.Size())
				{
					auto item = target.GetAt(sourceIndex);
					target.RemoveAt(sourceIndex);
					target.InsertAt(index, item);
				}
				else
				{
					target.InsertAt(index, values[index]);
				}
			}
			while (target.Size() > values.size())
				target.RemoveAtEnd();
		};

		std::vector<winrt::OpenNet::ViewModels::PeerDisplayItem> disconnecting;
		for (auto const& [key, item] : m_disconnectingPeers)
		{
			(void)key;
			disconnecting.push_back(item);
		}
		std::vector<winrt::OpenNet::ViewModels::PeerDisplayItem> banned;
		for (auto const& [key, item] : m_banIpPeers)
		{
			(void)key;
			banned.push_back(item);
		}

		SortPeerItems(connected);
		SortPeerItems(connecting);
		SortPeerItems(disconnecting);
		SortPeerItems(banned);
		replaceChildren(m_connectedGroup.Children(), connected);
		replaceChildren(m_connectingGroup.Children(), connecting);
		replaceChildren(m_disconnectingGroup.Children(), disconnecting);
		replaceChildren(m_banIpGroup.Children(), banned);

		m_connectedGroup.IP(
			L"bt_connected (" + winrt::to_hstring(connected.size()) + L")");
		m_connectingGroup.IP(
			L"bt_connecting (" + winrt::to_hstring(connecting.size()) + L")");
		m_disconnectingGroup.IP(
			L"disconnecting (" + winrt::to_hstring(disconnecting.size()) + L")");
		m_banIpGroup.IP(
			L"BanIP (" + winrt::to_hstring(banned.size()) + L")");

		auto const hasAny = !connected.empty() || !connecting.empty()
			|| !disconnecting.empty() || !banned.empty();
		if (emptyText)
			emptyText.Visibility(hasAny ? Visibility::Collapsed : Visibility::Visible);
	}

	// ---- Ban peer handlers ----

	void TaskPeersListPage::BanSelectedPeer(std::int64_t durationSeconds)
	{
		auto listView = PeersTreeView();
		if (!listView) return;
		auto selected = listView.SelectedItem();
		if (!selected) return;
		auto peer = selected.try_as<winrt::OpenNet::ViewModels::PeerDisplayItem>();
		if (!peer || peer.IsGroup()) return;

		auto const ip = winrt::to_string(peer.Address());
		auto const port = peer.Port();
		if (ip.empty())
			return;

		try
		{
			auto& ipFilter = ::OpenNet::Core::IPFilterManager::Instance();
			auto const taskId = m_viewModel && m_viewModel.SelectedTask()
				? winrt::to_string(m_viewModel.SelectedTask().TaskId())
				: std::string{};
			auto const now = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			auto const expiresAt =
				durationSeconds > 0 ? now + durationSeconds : 0;
			auto const id = ipFilter.AddBan(
				ip,
				port,
				taskId,
				winrt::to_string(peer.Client()),
				"manual",
				"user_manual",
				expiresAt);
			if (id == 0)
				throw std::runtime_error("failed to persist IP ban");
			ipFilter.ApplyToSession();
			RefreshPeerList();
			OutputDebugStringW(
				(L"Banned peer IP: " + winrt::to_hstring(ip) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("BanSelectedPeer: Error adding IP filter rule\n");
		}
	}

	void TaskPeersListPage::BanPeer1h_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		BanSelectedPeer(60 * 60);
	}

	void TaskPeersListPage::BanPeer24h_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		BanSelectedPeer(24 * 60 * 60);
	}

	void TaskPeersListPage::BanPeerPermanent_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		BanSelectedPeer(0);
	}

	void TaskPeersListPage::UnbanPeer_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto const selected = PeersTreeView().SelectedItem();
		if (!selected)
			return;
		auto const peer =
			selected.try_as<winrt::OpenNet::ViewModels::PeerDisplayItem>();
		if (!peer || peer.IsGroup() || peer.Address().empty())
			return;

		auto& ipFilter = ::OpenNet::Core::IPFilterManager::Instance();
		if (ipFilter.RemoveBan(winrt::to_string(peer.Address()), "manual"))
			RefreshPeerList();
	}
}
