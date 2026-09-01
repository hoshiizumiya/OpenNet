#include "XamlWorkaround.h"
import winrt.OpenNet.ViewModels;

#include "MainViewModel.h"
#include "ViewModels/MainViewModel.g.cpp"

import OpenNet.Core.DownloadManager;
import OpenNet.Core.P2PManager;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace std::chrono_literals;

namespace winrt::OpenNet::ViewModels::implementation
{
	// Summary: 构造函数，初始化默认状态和集合
	MainViewModel::MainViewModel()
		: m_isConnected(false), m_userName(L"Guest"), m_portState(L"Unknown")
	{
		m_dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
		m_recentActivities = single_threaded_observable_vector<hstring>();
		m_recentActivities.Append(L"应用已启动 / App started");
	}

	MainViewModel::~MainViewModel()
	{
		Shutdown();
	}

	void MainViewModel::Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(m_speedMutex);
			m_stopSpeedRefresh.store(true);
		}
		m_speedCv.notify_all();
		if (m_speedRefreshThread.joinable())
			m_speedRefreshThread.join();
		if (m_portRefreshThread.joinable())
			m_portRefreshThread.join();
	}

	// Summary: 初始化视图模型，设置就绪状态
	void MainViewModel::Initialize()
	{
		InitializeTorrentCore();

		// Start periodic speed refresh thread
		m_stopSpeedRefresh.store(false);
		m_speedRefreshThread = std::thread([this]()
		{
			SpeedRefreshThreadEntry();
		});
		m_portRefreshThread = std::thread([this]()
		{
			PortRefreshThreadEntry();
		});
	}


	IAsyncAction MainViewModel::InitializeTorrentCore()
	{
		// 使用单例管理 torrent 核心，异步后台初始化
		auto dispatcher = m_dispatcher;
		co_await ::OpenNet::Core::P2PManager::Instance().EnsureTorrentCoreInitializedAsync();
		if (::OpenNet::Core::P2PManager::Instance().IsTorrentCoreInitialized())
		{
			co_await winrtplus::resume_foreground(dispatcher);
		}
		else
		{
			co_await winrtplus::resume_foreground(dispatcher);
		}
	}

	// Format bytes/sec to a human-readable string
	static std::wstring FormatSpeed(std::uint64_t bytesPerSec)
	{
		double kbs = bytesPerSec / 1024.0;
		if (kbs > 1024.0)
			return std::format(L"{:.1f} MB/s", kbs / 1024.0);
		else if (kbs > 1.0)
			return std::format(L"{:.0f} KB/s", kbs);
		else
			return std::format(L"{} B/s", bytesPerSec);
	}

	void MainViewModel::SpeedRefreshThreadEntry()
	{
		// Wait for TorrentCore to finish initializing (up to 30s)
		// before polling session stats, otherwise listenPort/dhtNodes will be 0.
		for (int i = 0; i < 60 && !m_stopSpeedRefresh.load(); ++i)
		{
			if (::OpenNet::Core::P2PManager::Instance().IsTorrentCoreInitialized())
				break;
			std::unique_lock<std::mutex> lock(m_speedMutex);
			m_speedCv.wait_for(lock, 500ms, [this]
			{
				return m_stopSpeedRefresh.load();
			});
		}

		while (!m_stopSpeedRefresh.load())
		{
			try
			{
				// Gather HTTP speeds from Aria2
				auto& dlMgr = ::OpenNet::Core::DownloadManager::Instance();
				std::uint64_t httpDl = dlMgr.TotalHttpDownloadSpeed();
				std::uint64_t httpUl = dlMgr.TotalHttpUploadSpeed();

				// Gather BT speeds from P2PManager
				std::uint64_t btDl = 0;
				std::uint64_t btUl = 0;
				int peersCount = 0;
				int dhtNodes = 0;
				int listenPort = 0;
				int ipv4ListenPort = 0;
				int ipv6ListenPort = 0;
				if (::OpenNet::Core::P2PManager::Instance().IsTorrentCoreInitialized())
				{
					auto stats = ::OpenNet::Core::P2PManager::Instance().GetPerformanceStats();
					btDl = static_cast<std::uint64_t>(stats.totalDownloadRate);
					btUl = static_cast<std::uint64_t>(stats.totalUploadRate);
					peersCount = stats.numPeers;
					dhtNodes = stats.dhtNodes;
					listenPort = stats.listenPort;
					ipv4ListenPort = stats.ipv4ListenPort;
					ipv6ListenPort = stats.ipv6ListenPort;
				}

				std::uint64_t totalDl = httpDl + btDl;
				std::uint64_t totalUl = httpUl + btUl;

				auto speedText = std::format(L"\u2193 {} \u2191 {}",
											 FormatSpeed(totalDl), FormatSpeed(totalUl));

				// Determine speed level for SwitchPresenter icon
				double dlMBps = totalDl / (1024.0 * 1024.0);
				std::wstring speedLevelText;
				if (dlMBps >= 10.0)
					speedLevelText = L"High";
				else if (dlMBps >= 1.0)
					speedLevelText = L"Medium";
				else
					speedLevelText = L"Low";

				if (m_dispatcher)
				{
					auto hspeed = winrt::hstring{ speedText };
					auto hspeedLevel = winrt::hstring{ speedLevelText };
					auto peers = peersCount;
					auto dht = dhtNodes;
					auto port = listenPort;
					auto ipv4Port = ipv4ListenPort;
					auto ipv6Port = ipv6ListenPort;
					m_dispatcher.TryEnqueue([this, hspeed, hspeedLevel, peers, dht, port, ipv4Port, ipv6Port]()
					{
						SetProperty(m_currentTransferSpeedText, hspeed, L"CurrentTransferSpeedText");
						SetProperty(m_speedLevel, hspeedLevel, L"SpeedLevel");
						SetProperty(m_connectedPeersCount, peers, L"ConnectedPeersCount");
						SetProperty(m_dhtNodeCount, dht, L"DhtNodeCount");
						SetProperty(m_listenPort, port, L"ListenPort");
						SetProperty(m_ipv4ListenPort, ipv4Port, L"IPv4ListenPort");
						SetProperty(m_ipv6ListenPort, ipv6Port, L"IPv6ListenPort");
						SetProperty(
							m_listenPortText,
							port > 0 ? winrt::to_hstring(port) : winrt::hstring{ L"—" },
							L"ListenPortText");
						SetProperty(
							m_ipv4ListenPortText,
							ipv4Port > 0 ? winrt::to_hstring(ipv4Port) : winrt::hstring{ L"—" },
							L"IPv4ListenPortText");
						SetProperty(
							m_ipv6ListenPortText,
							ipv6Port > 0 ? winrt::to_hstring(ipv6Port) : winrt::hstring{ L"—" },
							L"IPv6ListenPortText");
					});
				}
			}
			catch (...)
			{
			}

			{
				std::unique_lock<std::mutex> lock(m_speedMutex);
				m_speedCv.wait_for(lock, 1500ms, [this]
				{
					return m_stopSpeedRefresh.load();
				});
			}
		}
	}

	void MainViewModel::PortRefreshThreadEntry()
	{
		bool apartmentInitialized = false;
		try
		{
			winrt::init_apartment(winrt::apartment_type::multi_threaded);
			apartmentInitialized = true;
		}
		catch (...)
		{
			// A probe failure is surfaced as Unknown/Timed out below. The speed,
			// DHT and listener refresh loop remains completely independent.
		}

		auto waitForProbe = [this](IAsyncAction const& operation)
		{
			auto status = operation.Status();
			while (status == AsyncStatus::Started)
			{
				if (m_stopSpeedRefresh.load())
				{
					operation.Cancel();
					return false;
				}
				std::unique_lock<std::mutex> lock(m_speedMutex);
				m_speedCv.wait_for(lock, 100ms, [this]
				{
					return m_stopSpeedRefresh.load();
				});
				status = operation.Status();
			}
			if (status != AsyncStatus::Completed)
				return false;
			try
			{
				operation.GetResults();
				return true;
			}
			catch (...)
			{
				return false;
			}
		};

		auto protocolState = [](
			::OpenNet::Core::PortProbeResult const& result,
			bool ipv6,
			bool tcp,
			int port) -> std::wstring
		{
			if (port <= 0)
				return L"Unavailable";
			bool const reachable = ipv6
				? (tcp ? result.ipv6TcpReachable : result.ipv6UdpReachable)
				: (tcp ? result.tcpReachable : result.udpReachable);
			bool const completed = ipv6
				? (tcp ? result.ipv6TcpCompleted : result.ipv6UdpCompleted)
				: (tcp ? result.tcpCompleted : result.udpCompleted);
			bool const timedOut = ipv6
				? result.ipv6TimedOut : result.ipv4TimedOut;
			if (reachable)
				return L"Open";
			if (completed)
				return L"Blocked";
			if (timedOut)
				return L"Timed out";
			return L"Unavailable";
		};
		auto familyState = [](std::wstring const& tcp, std::wstring const& udp)
		{
			if (tcp == L"Open" || udp == L"Open") return std::wstring{ L"Open" };
			if (tcp == L"Blocked" && udp == L"Blocked") return std::wstring{ L"Blocked" };
			if (tcp == L"Timed out" || udp == L"Timed out") return std::wstring{ L"Timed out" };
			if (tcp == L"Blocked" || udp == L"Blocked") return std::wstring{ L"Blocked" };
			return std::wstring{ L"Unavailable" };
		};

		while (!m_stopSpeedRefresh.load())
		{
			try
			{
				auto const stats = ::OpenNet::Core::P2PManager::Instance().GetPerformanceStats();
				auto const ipv4Port = stats.ipv4ListenPort;
				auto const ipv6Port = stats.ipv6ListenPort;
				auto const now = std::chrono::steady_clock::now();
				bool const due = ipv4Port != m_lastCheckedIPv4Port
					|| ipv6Port != m_lastCheckedIPv6Port
					|| now - m_lastPortCheckTime >= std::chrono::seconds(60);
				if (due)
				{
					std::wstring ipv4TcpState = ipv4Port > 0 ? L"Unknown" : L"Unavailable";
					std::wstring ipv4UdpState = ipv4TcpState;
					std::wstring ipv6TcpState = ipv6Port > 0 ? L"Unknown" : L"Unavailable";
					std::wstring ipv6UdpState = ipv6TcpState;
					if (ipv4Port > 0 && ipv4Port == ipv6Port)
					{
						auto result = std::make_shared<::OpenNet::Core::PortProbeResult>();
						auto operation = m_networkDetector.TestPortAccessibilityDetailedAsync(
							static_cast<std::uint16_t>(ipv4Port), result,
							::OpenNet::Core::PortProbeAddressFamily::Both);
						if (waitForProbe(operation))
						{
							ipv4TcpState = protocolState(*result, false, true, ipv4Port);
							ipv4UdpState = protocolState(*result, false, false, ipv4Port);
							ipv6TcpState = protocolState(*result, true, true, ipv6Port);
							ipv6UdpState = protocolState(*result, true, false, ipv6Port);
						}
						else if (!m_stopSpeedRefresh.load())
						{
							ipv4TcpState = ipv4UdpState = L"Timed out";
							ipv6TcpState = ipv6UdpState = L"Timed out";
						}
					}
					else
					{
						if (ipv4Port > 0)
						{
							auto result = std::make_shared<::OpenNet::Core::PortProbeResult>();
							auto operation = m_networkDetector.TestPortAccessibilityDetailedAsync(
								static_cast<std::uint16_t>(ipv4Port), result,
								::OpenNet::Core::PortProbeAddressFamily::IPv4);
							if (waitForProbe(operation))
							{
								ipv4TcpState = protocolState(*result, false, true, ipv4Port);
								ipv4UdpState = protocolState(*result, false, false, ipv4Port);
							}
							else
							{
								ipv4TcpState = ipv4UdpState = L"Timed out";
							}
						}
						if (ipv6Port > 0 && !m_stopSpeedRefresh.load())
						{
							auto result = std::make_shared<::OpenNet::Core::PortProbeResult>();
							auto operation = m_networkDetector.TestPortAccessibilityDetailedAsync(
								static_cast<std::uint16_t>(ipv6Port), result,
								::OpenNet::Core::PortProbeAddressFamily::IPv6);
							if (waitForProbe(operation))
							{
								ipv6TcpState = protocolState(*result, true, true, ipv6Port);
								ipv6UdpState = protocolState(*result, true, false, ipv6Port);
							}
							else
							{
								ipv6TcpState = ipv6UdpState = L"Timed out";
							}
						}
					}
					auto const ipv4State = familyState(ipv4TcpState, ipv4UdpState);
					auto const ipv6State = familyState(ipv6TcpState, ipv6UdpState);

					m_cachedIPv4PortState = ipv4State;
					m_cachedIPv6PortState = ipv6State;
					m_lastCheckedIPv4Port = ipv4Port;
					m_lastCheckedIPv6Port = ipv6Port;
					m_lastPortCheckTime = std::chrono::steady_clock::now();

					std::wstring aggregateState = L"Unknown";
					if (ipv4State == L"Open" || ipv6State == L"Open")
						aggregateState = L"Open";
					else if (ipv4State == L"Blocked" || ipv6State == L"Blocked")
						aggregateState = L"Blocked";
					if (m_dispatcher)
					{
						m_dispatcher.TryEnqueue([
							this,
							ipv4 = winrt::hstring{ ipv4State },
							ipv6 = winrt::hstring{ ipv6State },
							ipv4Tcp = winrt::hstring{ ipv4TcpState },
							ipv4Udp = winrt::hstring{ ipv4UdpState },
							ipv6Tcp = winrt::hstring{ ipv6TcpState },
							ipv6Udp = winrt::hstring{ ipv6UdpState },
							aggregate = winrt::hstring{ aggregateState }]()
						{
							SetProperty(m_ipv4PortState, ipv4, L"IPv4PortState");
							SetProperty(m_ipv6PortState, ipv6, L"IPv6PortState");
							SetProperty(m_ipv4TcpPortState, ipv4Tcp, L"IPv4TcpPortState");
							SetProperty(m_ipv4UdpPortState, ipv4Udp, L"IPv4UdpPortState");
							SetProperty(m_ipv6TcpPortState, ipv6Tcp, L"IPv6TcpPortState");
							SetProperty(m_ipv6UdpPortState, ipv6Udp, L"IPv6UdpPortState");
							SetProperty(m_portState, aggregate, L"PortState");
						});
					}
				}
			}
			catch (...)
			{
			}

			std::unique_lock<std::mutex> lock(m_speedMutex);
			m_speedCv.wait_for(lock, 1500ms, [this]
			{
				return m_stopSpeedRefresh.load();
			});
		}

		if (apartmentInitialized)
			winrt::uninit_apartment();
	}
}
