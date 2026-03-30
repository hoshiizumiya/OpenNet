/*
 * PROJECT:   OpenNet
 * FILE:      Core/Aria2/Aria2Engine.cpp
 * PURPOSE:   Aria2 engine management implementation
 *            (migrated from NanaGetCore.cpp)
 *
 * LICENSE:   The MIT License
 */

#define _WINSOCKAPI_

#include "pch.h"
#include "Core/Aria2/Aria2Engine.h"
#include "Core/Aria2/Aria2Helpers.h"
#include "Core/Aria2/JsonRpc2.h"

#include <Windows.h>
#include <objbase.h>
#include "Core/IO/FileSystem.h"

#include <WinSock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")

#undef GetObject

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>

using namespace OpenNet::Core::Aria2::Helpers;

namespace winrt_http = winrt::Windows::Web::Http;

// ===================================================================
//  Utility functions
// ===================================================================

std::filesystem::path OpenNet::Core::Aria2::GetApplicationFolderPath()
{
	static std::filesystem::path cached = ([]() -> std::filesystem::path
	{
		const DWORD bufSize = 32767;
		wchar_t buf[bufSize];
		::GetModuleFileNameW(nullptr, buf, bufSize);
		std::wcsrchr(buf, L'\\')[0] = L'\0';
		return std::filesystem::path(buf);
	}());
	return cached;
}

std::filesystem::path OpenNet::Core::Aria2::GetSettingsFolderPath()
{
	static std::filesystem::path cached = ([]() -> std::filesystem::path
	{
		std::filesystem::path fp(winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW());
		if (!IsPackagedMode())
		{
			fp /= L"Aria2";
		}
		std::filesystem::create_directories(fp);
		return fp;
	}());
	return cached;
}

winrt::hstring OpenNet::Core::Aria2::CreateGuidString()
{
	GUID guid;
	winrt::check_hresult(::CoCreateGuid(&guid));
	return winrt::to_hstring(guid);
}

winrt::hstring OpenNet::Core::Aria2::ConvertSecondsToTimeString(std::uint64_t Seconds)
{
	if (static_cast<uint64_t>(-1) == Seconds)
		return L"N/A";
	int h = static_cast<int>(Seconds / 3600);
	int m = static_cast<int>(Seconds / 60 % 60);
	int s = static_cast<int>(Seconds % 60);
	return winrt::hstring(FormatWideString(L"%d:%02d:%02d", h, m, s));
}

bool OpenNet::Core::Aria2::FindSubString(
	winrt::hstring const& SourceString,
	winrt::hstring const& SubString,
	bool IgnoreCase)
{
	return (::FindNLSStringEx(
		nullptr,
		(IgnoreCase ? NORM_IGNORECASE : 0) | FIND_FROMSTART,
		SourceString.c_str(), static_cast<int>(SourceString.size()),
		SubString.c_str(), static_cast<int>(SubString.size()),
		nullptr, nullptr, nullptr, 0) >= 0);
}

// ===================================================================
//  Aria2Instance
// ===================================================================

OpenNet::Core::Aria2::Aria2Instance::Aria2Instance(
	winrt::Windows::Foundation::Uri const& ServerUri,
	std::string const& ServerToken)
	: m_HttpClient(winrt_http::HttpClient())
{
	UpdateInstance(ServerUri, ServerToken);
}

OpenNet::Core::Aria2::Aria2Instance::Aria2Instance()
	: m_HttpClient(winrt_http::HttpClient())
{
}

OpenNet::Core::Aria2::Aria2Instance::~Aria2Instance()
{
	m_HttpClient.Close();
}

winrt::Windows::Foundation::Uri OpenNet::Core::Aria2::Aria2Instance::ServerUri()
{
	return m_ServerUri;
}

std::string OpenNet::Core::Aria2::Aria2Instance::ServerToken()
{
	return m_ServerToken;
}

void OpenNet::Core::Aria2::Aria2Instance::UpdateInstance(
	winrt::Windows::Foundation::Uri const& ServerUri,
	std::string const& ServerToken)
{
	m_ServerUri = ServerUri;
	m_ServerToken = ServerToken;
}

// ----- Commands -----

void OpenNet::Core::Aria2::Aria2Instance::Shutdown(bool Force)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	if ("\"OK\"" != SimpleJsonRpcCall(
		Force ? "aria2.forceShutdown" : "aria2.shutdown",
		params.dump(2)))
	{
		throw winrt::hresult_error();
	}
}

void OpenNet::Core::Aria2::Aria2Instance::PauseAll(bool Force)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	if ("\"OK\"" != SimpleJsonRpcCall(
		Force ? "aria2.forcePauseAll" : "aria2.pauseAll", params.dump(2)))
	{
		throw winrt::hresult_error();
	}
}

void OpenNet::Core::Aria2::Aria2Instance::ResumeAll()
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	if ("\"OK\"" != SimpleJsonRpcCall("aria2.unpauseAll", params.dump(2)))
	{
		throw winrt::hresult_error();
	}
}

void OpenNet::Core::Aria2::Aria2Instance::ClearList()
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	if ("\"OK\"" != SimpleJsonRpcCall("aria2.purgeDownloadResult", params.dump(2)))
	{
		throw winrt::hresult_error();
	}
}

void OpenNet::Core::Aria2::Aria2Instance::Pause(std::string const& Gid, bool Force)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	params.push_back(Gid);
	SimpleJsonRpcCall(Force ? "aria2.forcePause" : "aria2.pause", params.dump(2));
}

void OpenNet::Core::Aria2::Aria2Instance::Resume(std::string const& Gid)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	params.push_back(Gid);
	SimpleJsonRpcCall("aria2.unpause", params.dump(2));
}

void OpenNet::Core::Aria2::Aria2Instance::Cancel(std::string const& Gid, bool Force)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	params.push_back(Gid);
	if (std::string("\"") + Gid + "\"" != SimpleJsonRpcCall(
		Force ? "aria2.forceRemove" : "aria2.remove", params.dump(2)))
	{
		throw winrt::hresult_error();
	}
}

void OpenNet::Core::Aria2::Aria2Instance::Remove(std::string const& Gid)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	params.push_back(Gid);
	if ("\"OK\"" != SimpleJsonRpcCall("aria2.removeDownloadResult", params.dump(2)))
	{
		throw winrt::hresult_error();
	}
}

std::string OpenNet::Core::Aria2::Aria2Instance::AddUri(std::string const& Source)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);
	nlohmann::json uris;
	uris.push_back(Source);
	params.push_back(uris);
	return SimpleJsonRpcCall("aria2.addUri", params.dump(2));
}

std::string OpenNet::Core::Aria2::Aria2Instance::AddUriWithOptions(
	std::string const& Source,
	std::string const& Dir,
	std::string const& OutFileName)
{
	nlohmann::json params;
	params.push_back("token:" + m_ServerToken);

	nlohmann::json uris;
	uris.push_back(Source);
	params.push_back(uris);

	nlohmann::json options;
	if (!Dir.empty())
		options["dir"] = Dir;
	if (!OutFileName.empty())
		options["out"] = OutFileName;
	params.push_back(options);

	return SimpleJsonRpcCall("aria2.addUri", params.dump(2));
}

// ----- Status -----

std::mutex& OpenNet::Core::Aria2::Aria2Instance::InstanceLock()
{
	return m_InstanceLock;
}

std::uint64_t OpenNet::Core::Aria2::Aria2Instance::TotalDownloadSpeed()
{
	return m_GlobalStatus.DownloadSpeed;
}

std::uint64_t OpenNet::Core::Aria2::Aria2Instance::TotalUploadSpeed()
{
	return m_GlobalStatus.UploadSpeed;
}

std::size_t OpenNet::Core::Aria2::Aria2Instance::NumActive()
{
	return m_GlobalStatus.NumActive;
}

std::size_t OpenNet::Core::Aria2::Aria2Instance::NumWaiting()
{
	return m_GlobalStatus.NumWaiting;
}

std::size_t OpenNet::Core::Aria2::Aria2Instance::NumStopped()
{
	return m_GlobalStatus.NumStopped;
}

void OpenNet::Core::Aria2::Aria2Instance::RefreshInformation()
{
	try
	{
		nlohmann::json params;
		params.push_back("token:" + m_ServerToken);
		auto response = nlohmann::json::parse(
			SimpleJsonRpcCall("aria2.getGlobalStat", params.dump(2)));
		m_GlobalStatus = ToGlobalStatusInformation(response);
	}
	catch (...)
	{
	}
}

OpenNet::Core::Aria2::DownloadInformation
OpenNet::Core::Aria2::Aria2Instance::GetTaskInformation(std::string const& Gid)
{
	nlohmann::json params;
	params.emplace_back("token:" + m_ServerToken);
	params.emplace_back(Gid);
	auto response = nlohmann::json::parse(
		SimpleJsonRpcCall("aria2.tellStatus", params.dump(2)));
	return ToDownloadInformation(response);
}

std::vector<std::string> OpenNet::Core::Aria2::Aria2Instance::GetTaskList()
{
	std::vector<std::string> result;

	// Active tasks
	{
		nlohmann::json params;
		params.emplace_back("token:" + m_ServerToken);
		nlohmann::json keys;
		keys.emplace_back("gid");
		params.emplace_back(keys);
		auto response = nlohmann::json::parse(
			SimpleJsonRpcCall("aria2.tellActive", params.dump(2)));
		for (auto const& task : response)
			result.emplace_back(task["gid"].get<std::string>());
	}

	// Waiting tasks
	{
		nlohmann::json params;
		params.emplace_back("token:" + m_ServerToken);
		params.emplace_back(0);
		params.emplace_back(m_GlobalStatus.NumWaiting);
		nlohmann::json keys;
		keys.emplace_back("gid");
		params.emplace_back(keys);
		auto response = nlohmann::json::parse(
			SimpleJsonRpcCall("aria2.tellWaiting", params.dump(2)));
		for (auto const& task : response)
			result.emplace_back(task["gid"].get<std::string>());
	}

	// Stopped tasks
	{
		nlohmann::json params;
		params.emplace_back("token:" + m_ServerToken);
		params.emplace_back(0);
		params.emplace_back(m_GlobalStatus.NumStopped);
		nlohmann::json keys;
		keys.emplace_back("gid");
		params.emplace_back(keys);
		auto response = nlohmann::json::parse(
			SimpleJsonRpcCall("aria2.tellStopped", params.dump(2)));
		for (auto const& task : response)
			result.emplace_back(task["gid"].get<std::string>());
	}

	return result;
}

// ----- HTTP transport -----

std::string OpenNet::Core::Aria2::Aria2Instance::SimplePost(std::string const& Content)
{
	auto response = m_HttpClient.PostAsync(
		m_ServerUri,
		winrt_http::HttpStringContent(winrt::to_hstring(Content)))
		.get();

	auto buffer = response.Content().ReadAsBufferAsync().get();
	if (!response.IsSuccessStatusCode() && !buffer.Length())
	{
		response.EnsureSuccessStatusCode();
	}

	return std::string(
		reinterpret_cast<char*>(buffer.data()),
		buffer.Length());
}

std::string OpenNet::Core::Aria2::Aria2Instance::SimpleJsonRpcCall(
	std::string const& MethodName,
	std::string const& Parameters)
{
	JsonRpc2::RequestMessage request;
	request.Method = MethodName;
	request.Parameters = Parameters;
	request.Identifier = winrt::to_string(CreateGuidString());

	std::string rawReq = JsonRpc2::FromRequestMessage(request);
	if (rawReq.empty())
	{
		throw winrt::hresult_invalid_argument(L"Invalid JSON-RPC 2.0 request.");
	}

	JsonRpc2::ResponseMessage response;
	if (!JsonRpc2::ToResponseMessage(SimplePost(rawReq), response) ||
		response.Identifier != request.Identifier)
	{
		throw winrt::hresult_illegal_method_call(L"Invalid JSON-RPC 2.0 response.");
	}

	if (!response.IsSucceeded)
	{
		throw winrt::hresult_illegal_method_call(winrt::to_hstring(response.Message));
	}

	return response.Message;
}

// ===================================================================
//  LocalAria2Instance
// ===================================================================

constexpr std::string_view BitTorrentTrackers =
"http://1337.abcvg.info:80/announce,"
"http://nyaa.tracker.wf:7777/announce,"
"http://open.acgnxtracker.com:80/announce,"
"http://opentracker.xyz:80/announce,"
"http://share.camoe.cn:8080/announce,"
"http://tracker.bt4g.com:2095/announce,"
"http://tracker.files.fm:6969/announce,"
"http://tracker.gbitt.info:80/announce,"
"https://opentracker.i2p.rocks:443/announce,"
"https://tracker.lilithraws.cf:443/announce,"
"https://tracker.nanoha.org:443/announce,"
"https://tracker.tamersunion.org:443/announce,"
"udp://bt1.archive.org:6969/announce,"
"udp://exodus.desync.com:6969/announce,"
"udp://open.stealth.si:80/announce,"
"udp://open.tracker.ink:6969/announce,"
"udp://opentracker.i2p.rocks:6969/announce,"
"udp://p4p.arenabg.com:1337/announce,"
"udp://tracker.dler.org:6969/announce,"
"udp://tracker.dump.cl:6969/announce,"
"udp://tracker.openbittorrent.com:6969/announce,"
"udp://tracker.opentrackr.org:1337/announce,"
"udp://tracker.torrent.eu.org:451/announce,"
"udp://tracker2.dler.com:80/announce";

OpenNet::Core::Aria2::LocalAria2Instance::LocalAria2Instance()
{
	m_JobObjectHandle.attach(::CreateJobObjectW(nullptr, nullptr));
	if (!m_JobObjectHandle)
	{
		winrt::throw_last_error();
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION extInfo = { 0 };
	extInfo.BasicLimitInformation.LimitFlags =
		JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
		JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
	winrt::check_bool(::SetInformationJobObject(
		m_JobObjectHandle.get(),
		JobObjectExtendedLimitInformation,
		&extInfo,
		sizeof(extInfo)));
}

OpenNet::Core::Aria2::LocalAria2Instance::~LocalAria2Instance()
{
	// Destructor may run during atexit on the main (STA) thread.
	// Only force-terminate the process — no RPC calls (.get()) allowed.
	// Graceful Terminate() (RPC Shutdown + wait) should be called explicitly
	// from a background thread before destruction.
	try
	{
		ForceTerminate();
	}
	catch (...)
	{
	}
}

winrt::Windows::Foundation::IAsyncAction OpenNet::Core::Aria2::LocalAria2Instance::RestartAsync()
{
	Terminate();
	co_await StartupAsync();
}

bool OpenNet::Core::Aria2::LocalAria2Instance::Available() const
{
	return m_Available;
}

std::uint16_t OpenNet::Core::Aria2::LocalAria2Instance::PickUnusedTcpPort()
{
	std::uint16_t result = 0;
	WSADATA wsaData{};
	if (NO_ERROR == ::WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock != INVALID_SOCKET)
		{
			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = INADDR_ANY;
			addr.sin_port = ::htons(0);
			if (SOCKET_ERROR != ::bind(sock, reinterpret_cast<LPSOCKADDR>(&addr), sizeof(addr)))
			{
				int nameLen = sizeof(addr);
				if (SOCKET_ERROR != ::getsockname(sock, reinterpret_cast<LPSOCKADDR>(&addr), &nameLen))
				{
					result = ::ntohs(addr.sin_port);
				}
			}
			::closesocket(sock);
		}
		::WSACleanup();
	}
	return result;
}

winrt::Windows::Foundation::IAsyncAction OpenNet::Core::Aria2::LocalAria2Instance::StartupAsync()
{
	if (m_Available)
		throw winrt::hresult_illegal_method_call();

	std::uint16_t serverPort = PickUnusedTcpPort();
	winrt::hstring serverToken = CreateGuidString();

	auto exitGuard = MakeScopeExit([this]()
	{
		if (!m_Available) ForceTerminate();
	});

	// Find aria2c executable – try several known locations
	std::filesystem::path aria2Exe;
	std::filesystem::path appDir = GetApplicationFolderPath();

	// Check for bundled aria2 executables
	for (auto const& name : { L"aria2c.exe", L"Mile.Aria2.exe" })
	{
		auto candidate = appDir / name;
		if (std::filesystem::exists(candidate))
		{
			aria2Exe = candidate;
			break;
		}
	}

	// If not found next to the executable, try PATH
	if (aria2Exe.empty())
	{
		wchar_t pathBuf[MAX_PATH]{};
		DWORD len = ::SearchPathW(nullptr, L"aria2c.exe", nullptr, MAX_PATH, pathBuf, nullptr);
		if (len > 0)
		{
			aria2Exe = std::filesystem::path(pathBuf);
		}
	}

	if (aria2Exe.empty() || !std::filesystem::exists(aria2Exe))
	{
		OutputDebugStringW(L"[Aria2Engine] aria2c.exe not found. HTTP downloads will not be available.\n");
		co_return; // gracefully degrade – let m_Available stay false
	}

	std::filesystem::path logFile = GetSettingsFolderPath() / L"aria2c.log";
	std::filesystem::path sessionFile = GetSettingsFolderPath() / L"download.session";
	std::filesystem::path dhtFile = GetSettingsFolderPath() / L"dht.dat";
	std::filesystem::path dht6File = GetSettingsFolderPath() / L"dht6.dat";

	// Async await GetDownloadsPathW instead of blocking .GetResults()
	winrt::hstring downloadsPath = co_await winrt::OpenNet::Core::IO::FileSystem::GetDownloadsPathW();

	std::vector<std::pair<std::wstring, std::wstring>> settings;
	settings.emplace_back(L"log", logFile.wstring());
	settings.emplace_back(L"log-level", L"notice");
	settings.emplace_back(L"enable-rpc", L"true");
	settings.emplace_back(L"rpc-listen-port", winrt::to_hstring(serverPort).c_str());
	settings.emplace_back(L"rpc-secret", serverToken.c_str());
	settings.emplace_back(L"dir", downloadsPath.c_str());
	settings.emplace_back(L"continue", L"true");
	settings.emplace_back(L"auto-save-interval", L"1");
	if (std::filesystem::exists(sessionFile))
	{
		settings.emplace_back(L"input-file", sessionFile.wstring());
	}
	settings.emplace_back(L"save-session", sessionFile.wstring());
	settings.emplace_back(L"save-session-interval", L"1");
	settings.emplace_back(L"dht-file-path", dhtFile.wstring());
	settings.emplace_back(L"dht-file-path6", dht6File.wstring());
	settings.emplace_back(L"bt-tracker", winrt::to_hstring(BitTorrentTrackers).c_str());

	// Build command line
	std::wstring cmdLine = L"\"" + aria2Exe.wstring() + L"\"";
	for (auto const& set : settings)
	{
		cmdLine += L" --" + set.first;
		if (!set.second.empty())
			cmdLine += L"=" + set.second;
	}

	STARTUPINFOW si = { sizeof(si) };
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdInput = INVALID_HANDLE_VALUE;
	si.hStdOutput = INVALID_HANDLE_VALUE;
	si.hStdError = INVALID_HANDLE_VALUE;

	PROCESS_INFORMATION pi{};
	if (!::CreateProcessW(
		nullptr,
		const_cast<LPWSTR>(cmdLine.c_str()),
		nullptr, nullptr, TRUE,
		CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
		nullptr, nullptr, &si, &pi))
	{
		winrt::throw_last_error();
	}

	winrt::check_bool(::AssignProcessToJobObject(m_JobObjectHandle.get(), pi.hProcess));
	m_ProcessHandle.attach(pi.hProcess);
	::ResumeThread(pi.hThread);
	::CloseHandle(pi.hThread);

	UpdateInstance(
		winrt::Windows::Foundation::Uri(
			winrt::hstring(FormatWideString(L"http://localhost:%d/jsonrpc", serverPort))),
		winrt::to_string(serverToken));

	m_Available = true;
	exitGuard.Dismiss();
}

void OpenNet::Core::Aria2::LocalAria2Instance::ForceTerminate()
{
	UpdateInstance(nullptr, {});
	if (m_ProcessHandle)
	{
		::TerminateProcess(m_ProcessHandle.get(), 0);
		m_ProcessHandle.close();
	}
	m_Available = false;
}

void OpenNet::Core::Aria2::LocalAria2Instance::Terminate()
{
	if (!m_Available)
		return;

	try
	{
		Shutdown();
	}
	catch (...)
	{
	}
	::WaitForSingleObjectEx(m_ProcessHandle.get(), 30 * 1000, FALSE);
	ForceTerminate();
}
