#include "pch.h"

#include <CppUnitTest.h>
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <libtorrent/create_torrent.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace OpenNetUnitTest
{
	namespace
	{
		namespace lt = libtorrent;
		using namespace std::chrono_literals;

		std::string Utf8Path(std::filesystem::path const& path)
		{
			auto const value = path.u8string();
			return {
				reinterpret_cast<char const*>(value.data()),
				value.size()
			};
		}

		void SendAll(SOCKET const socket, char const* data, std::size_t size)
		{
			while (size != 0)
			{
				auto const sent = ::send(
					socket,
					data,
					static_cast<int>((std::min)(
						size,
						static_cast<std::size_t>((std::numeric_limits<int>::max)()))),
					0);
				if (sent <= 0)
					throw std::runtime_error("HTTP test server send failed");
				data += sent;
				size -= static_cast<std::size_t>(sent);
			}
		}

		class RangeHttpServer
		{
		public:
			explicit RangeHttpServer(std::vector<std::uint8_t> bytes)
				: m_bytes(std::move(bytes))
			{
				Start();
			}

			explicit RangeHttpServer(
				std::unordered_map<std::string, std::vector<std::uint8_t>> routes)
				: m_routes(std::move(routes))
			{
				Start();
			}

		private:
			void Start()
			{
				WSADATA data{};
				if (::WSAStartup(MAKEWORD(2, 2), &data) != 0)
					throw std::runtime_error("WSAStartup failed");

				m_listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (m_listener == INVALID_SOCKET)
					throw std::runtime_error("socket failed");

				sockaddr_in address{};
				address.sin_family = AF_INET;
				address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
				address.sin_port = 0;
				if (::bind(
					m_listener,
					reinterpret_cast<sockaddr*>(&address),
					sizeof(address)) == SOCKET_ERROR)
					throw std::runtime_error("bind failed");
				if (::listen(m_listener, SOMAXCONN) == SOCKET_ERROR)
					throw std::runtime_error("listen failed");

				int addressSize = sizeof(address);
				if (::getsockname(
					m_listener,
					reinterpret_cast<sockaddr*>(&address),
					&addressSize) == SOCKET_ERROR)
					throw std::runtime_error("getsockname failed");
				m_port = ntohs(address.sin_port);

				m_thread = std::jthread([this](std::stop_token const token)
				{
					Run(token);
				});
			}

		public:
			~RangeHttpServer()
			{
				if (m_thread.joinable())
				m_thread.request_stop();
				if (m_listener != INVALID_SOCKET)
				{
					::closesocket(m_listener);
					m_listener = INVALID_SOCKET;
				}
				if (m_thread.joinable())
					m_thread.join();
				::WSACleanup();
			}

			std::string Url(std::string_view const path) const
			{
				return std::format(
					"http://127.0.0.1:{}{}",
					m_port,
					path);
			}

			std::vector<std::string> Targets() const
			{
				std::lock_guard lock(m_mutex);
				return m_targets;
			}

			std::vector<std::string> Ranges() const
			{
				std::lock_guard lock(m_mutex);
				return m_ranges;
			}

		private:
			std::vector<std::uint8_t> const* PayloadFor(
				std::string const& target) const
			{
				if (m_routes.empty())
					return &m_bytes;
				auto const route = m_routes.find(target);
				return route == m_routes.end() ? nullptr : &route->second;
			}

			void Run(std::stop_token const token)
			{
				while (!token.stop_requested())
				{
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(m_listener, &readSet);
					timeval timeout{};
					timeout.tv_sec = 0;
					timeout.tv_usec = 200000;
					auto const ready = ::select(0, &readSet, nullptr, nullptr, &timeout);
					if (ready <= 0)
						continue;

					auto client = ::accept(m_listener, nullptr, nullptr);
					if (client == INVALID_SOCKET)
						continue;
					try
					{
						Handle(client);
					}
					catch (...)
					{
					}
					::shutdown(client, SD_BOTH);
					::closesocket(client);
				}
			}

			void Handle(SOCKET const client)
			{
				std::string request;
				std::array<char, 4096> buffer{};
				while (!request.contains("\r\n\r\n"))
				{
					auto const received = ::recv(
						client,
						buffer.data(),
						static_cast<int>(buffer.size()),
						0);
					if (received <= 0)
						return;
					request.append(
						buffer.data(),
						static_cast<std::size_t>(received));
					if (request.size() > 64 * 1024)
						return;
				}

				auto const lineEnd = request.find("\r\n");
				auto const firstLine = request.substr(0, lineEnd);
				auto const firstSpace = firstLine.find(' ');
				auto const secondSpace = firstLine.find(' ', firstSpace + 1);
				if (firstSpace == std::string::npos
					|| secondSpace == std::string::npos)
					return;

				auto const method = firstLine.substr(0, firstSpace);
				auto const target = firstLine.substr(
					firstSpace + 1,
					secondSpace - firstSpace - 1);

				std::string range;
				auto const rangeHeader = request.find("\r\nRange:");
				if (rangeHeader != std::string::npos)
				{
					auto valueStart = rangeHeader + 8;
					while (valueStart < request.size()
						&& (request[valueStart] == ' '
							|| request[valueStart] == '\t'))
						++valueStart;
					auto const valueEnd = request.find("\r\n", valueStart);
					range = request.substr(
						valueStart,
						valueEnd - valueStart);
				}

				{
					std::lock_guard lock(m_mutex);
					m_targets.push_back(target);
					m_ranges.push_back(range);
				}

				auto const payload = PayloadFor(target);
				if (!payload)
				{
					constexpr std::string_view notFound =
						"HTTP/1.1 404 Not Found\r\n"
						"Content-Length: 0\r\n"
						"Connection: close\r\n\r\n";
					SendAll(client, notFound.data(), notFound.size());
					return;
				}

				if (method == "HEAD")
				{
					auto const header = std::format(
						"HTTP/1.1 200 OK\r\n"
						"Content-Length: {}\r\n"
						"Accept-Ranges: bytes\r\n"
						"Connection: close\r\n\r\n",
						payload->size());
					SendAll(client, header.data(), header.size());
					return;
				}
				if (payload->empty())
					return;

				std::size_t begin = 0;
				std::size_t end = payload->size() - 1;
				bool partial = false;
				if (range.starts_with("bytes="))
				{
					auto const dash = range.find('-', 6);
					if (dash != std::string::npos)
					{
						begin = static_cast<std::size_t>(
							std::stoull(range.substr(6, dash - 6)));
						if (dash + 1 < range.size())
							end = static_cast<std::size_t>(
								std::stoull(range.substr(dash + 1)));
						end = (std::min)(end, payload->size() - 1);
						partial = begin <= end && end < payload->size();
					}
				}

				if (!partial)
				{
					begin = 0;
					end = payload->size() - 1;
				}
				auto const length = end - begin + 1;
				auto const header = partial
					? std::format(
						"HTTP/1.1 206 Partial Content\r\n"
						"Content-Length: {}\r\n"
						"Content-Range: bytes {}-{}/{}\r\n"
						"Accept-Ranges: bytes\r\n"
						"Connection: close\r\n\r\n",
						length,
						begin,
						end,
						payload->size())
					: std::format(
						"HTTP/1.1 200 OK\r\n"
						"Content-Length: {}\r\n"
						"Accept-Ranges: bytes\r\n"
						"Connection: close\r\n\r\n",
						length);
				SendAll(client, header.data(), header.size());
				SendAll(
					client,
					reinterpret_cast<char const*>(payload->data() + begin),
					length);
			}

			std::vector<std::uint8_t> m_bytes;
			std::unordered_map<std::string, std::vector<std::uint8_t>> m_routes;
			mutable std::mutex m_mutex;
			std::vector<std::string> m_targets;
			std::vector<std::string> m_ranges;
			SOCKET m_listener{ INVALID_SOCKET };
			std::uint16_t m_port{};
			std::jthread m_thread;
		};

		struct CanonicalFixture
		{
			std::filesystem::path root;
			std::vector<std::uint8_t> bytes;
			std::vector<char> metainfo;

			CanonicalFixture() = default;
			CanonicalFixture(CanonicalFixture const&) = delete;
			CanonicalFixture& operator=(CanonicalFixture const&) = delete;
			CanonicalFixture(CanonicalFixture&& other) noexcept
				: root(std::move(other.root)),
				bytes(std::move(other.bytes)),
				metainfo(std::move(other.metainfo))
			{
				other.root.clear();
			}
			CanonicalFixture& operator=(CanonicalFixture&&) = delete;

			~CanonicalFixture()
			{
				if (root.empty())
					return;
				std::error_code error;
				std::filesystem::remove_all(root, error);
			}
		};

		CanonicalFixture MakeCanonicalFixture()
		{
			static std::atomic_uint64_t nextFixtureId{};
			auto const fixtureId =
				nextFixtureId.fetch_add(1, std::memory_order_relaxed);

			CanonicalFixture fixture;
			fixture.root =
				std::filesystem::temp_directory_path()
				/ std::filesystem::path{
					std::format(
						L"OpenNet-WebSeedTest-{}-{}",
						::GetCurrentProcessId(),
						fixtureId) };
			std::filesystem::remove_all(fixture.root);
			auto const sourceRoot = fixture.root / L"seed";
			auto const canonicalDirectory =
				sourceRoot / L"OpenNet.Content.v1";
			std::filesystem::create_directories(canonicalDirectory);

			fixture.bytes.resize(2 * 1024 * 1024 + 12345);
			for (std::size_t index = 0; index < fixture.bytes.size(); ++index)
				fixture.bytes[index] =
					static_cast<std::uint8_t>((index * 131u + 17u) & 0xffu);

			auto const sourceFile = canonicalDirectory / L"content";
			std::ofstream output(sourceFile, std::ios::binary);
			if (!output)
				throw std::runtime_error("failed to create canonical source file");
			output.write(
				reinterpret_cast<char const*>(fixture.bytes.data()),
				static_cast<std::streamsize>(fixture.bytes.size()));
			output.close();

			std::vector<lt::create_file_entry> files;
			files.push_back({
				"OpenNet.Content.v1/content",
				static_cast<std::int64_t>(fixture.bytes.size()),
				{},
				0,
				{}
			});
			lt::create_torrent creator(
				std::move(files),
				1024 * 1024,
				lt::create_torrent::v2_only);
			creator.set_creation_date(0);
			lt::error_code error;
			lt::set_piece_hashes(
				creator,
				Utf8Path(sourceRoot),
				error);
			if (error)
				throw std::runtime_error(error.message());
			fixture.metainfo = creator.generate_buf();
			return fixture;
		}

		void DownloadMetainfoFromWebSeed(
			std::vector<char> const& metainfo,
			std::filesystem::path const& outputRoot,
			std::string const& webSeed,
			std::optional<std::pair<lt::file_index_t, std::string>> rename = std::nullopt)
		{
			auto params = lt::load_torrent_buffer(
				lt::span<char const>(
					metainfo.data(),
					metainfo.size()));
			if (!params.ti)
				throw std::runtime_error("failed to load test torrent");

			std::filesystem::create_directories(outputRoot);
			params.save_path = Utf8Path(outputRoot);
			if (rename)
				params.renamed_files.insert_or_assign(
					rename->first,
					rename->second);
			params.url_seeds.push_back(webSeed);
			params.flags |=
				lt::torrent_flags::disable_dht
				| lt::torrent_flags::disable_lsd
				| lt::torrent_flags::disable_pex;
			params.flags &= ~lt::torrent_flags::auto_managed;
			params.flags &= ~lt::torrent_flags::paused;

			lt::settings_pack settings;
			settings.set_bool(
				lt::settings_pack::ssrf_mitigation,
				false);
			settings.set_str(
				lt::settings_pack::listen_interfaces,
				"127.0.0.1:0");

			lt::session_params sessionParameters;
			sessionParameters.settings = std::move(settings);
			lt::session session{ std::move(sessionParameters) };
			lt::error_code error;
			auto handle = session.add_torrent(params, error);
			if (error || !handle.is_valid())
				throw std::runtime_error(
					error ? error.message() : "invalid torrent handle");

			auto const deadline =
				std::chrono::steady_clock::now() + 30s;
			bool completed = false;
			while (std::chrono::steady_clock::now() < deadline)
			{
				auto const status = handle.status(
					lt::torrent_handle::query_accurate_download_counters);
				if (status.errc)
					throw std::runtime_error(status.errc.message());
				if (status.is_finished || status.is_seeding)
				{
					completed = true;
					break;
				}
				std::this_thread::sleep_for(50ms);
			}
			if (!completed)
				throw std::runtime_error("web-seed download timed out");
		}

		std::filesystem::path DownloadFromWebSeed(
			CanonicalFixture const& fixture,
			std::string const& webSeed)
		{
			auto const outputRoot = fixture.root / L"download";
			DownloadMetainfoFromWebSeed(
				fixture.metainfo,
				outputRoot,
				webSeed,
				std::pair{
					lt::file_index_t{0},
					std::string{"download.bin"} });
			return outputRoot / L"download.bin";
		}
		struct MultiFileV2Fixture
		{
			std::filesystem::path root;
			std::vector<std::uint8_t> first;
			std::vector<std::uint8_t> second;
			std::vector<char> metainfo;

			MultiFileV2Fixture() = default;
			MultiFileV2Fixture(MultiFileV2Fixture const&) = delete;
			MultiFileV2Fixture& operator=(MultiFileV2Fixture const&) = delete;
			MultiFileV2Fixture(MultiFileV2Fixture&& other) noexcept
				: root(std::move(other.root)),
				first(std::move(other.first)),
				second(std::move(other.second)),
				metainfo(std::move(other.metainfo))
			{
				other.root.clear();
			}
			MultiFileV2Fixture& operator=(MultiFileV2Fixture&&) = delete;

			~MultiFileV2Fixture()
			{
				std::error_code error;
				std::filesystem::remove_all(root, error);
			}
		};

		MultiFileV2Fixture MakeMultiFileV2BoundaryFixture()
		{
			static std::atomic_uint64_t nextFixtureId{};
			auto const fixtureId =
				nextFixtureId.fetch_add(1, std::memory_order_relaxed);

			MultiFileV2Fixture fixture;
			fixture.root =
				std::filesystem::temp_directory_path()
				/ std::filesystem::path{
					std::format(
						L"OpenNet-WebSeedV2Boundary-{}-{}",
						::GetCurrentProcessId(),
						fixtureId) };
			std::filesystem::remove_all(fixture.root);

			auto const sourceRoot = fixture.root / L"seed";
			auto const sourceDirectory =
				sourceRoot / L"v2-boundary";
			std::filesystem::create_directories(sourceDirectory);

			fixture.first.resize(1024 * 1024 + 12345);
			fixture.second.resize(1024 * 1024 + 23456);
			for (std::size_t index = 0; index < fixture.first.size(); ++index)
				fixture.first[index] =
					static_cast<std::uint8_t>((index * 17u + 11u) & 0xffu);
			for (std::size_t index = 0; index < fixture.second.size(); ++index)
				fixture.second[index] =
					static_cast<std::uint8_t>((index * 29u + 7u) & 0xffu);

			auto writeFile = [](std::filesystem::path const& path,
				std::vector<std::uint8_t> const& bytes)
			{
				std::ofstream output(path, std::ios::binary);
				if (!output)
					throw std::runtime_error(
						"failed to create multi-file WebSeed fixture");
				output.write(
					reinterpret_cast<char const*>(bytes.data()),
					static_cast<std::streamsize>(bytes.size()));
			};
			writeFile(sourceDirectory / L"first.bin", fixture.first);
			writeFile(sourceDirectory / L"second.bin", fixture.second);

			std::vector<lt::create_file_entry> files;
			files.emplace_back(
				"v2-boundary/first.bin",
				static_cast<std::int64_t>(fixture.first.size()));
			files.emplace_back(
				"v2-boundary/second.bin",
				static_cast<std::int64_t>(fixture.second.size()));
			lt::create_torrent creator(
				std::move(files),
				1024 * 1024,
				lt::create_torrent::v2_only);
			creator.set_creation_date(0);
			lt::error_code error;
			lt::set_piece_hashes(
				creator,
				Utf8Path(sourceRoot),
				error);
			if (error)
				throw std::runtime_error(error.message());
			fixture.metainfo = creator.generate_buf();
			return fixture;
		}
	}

	TEST_CLASS(CanonicalWebSeedTests)
	{
	public:
		TEST_METHOD(DirectFileUrlSeedDoesNotAppendCanonicalPath)
		{
			auto fixture = MakeCanonicalFixture();
			RangeHttpServer server{ fixture.bytes };
			auto const output = DownloadFromWebSeed(
				fixture,
				server.Url("/file.bin"));

			std::ifstream input(output, std::ios::binary);
			std::vector<std::uint8_t> downloaded(
				std::istreambuf_iterator<char>{ input },
				std::istreambuf_iterator<char>{});
			Assert::IsTrue(
				downloaded == fixture.bytes,
				L"libtorrent output must match the canonical source bytes");

			auto const targets = server.Targets();
			Assert::IsFalse(
				targets.empty(),
				L"the web seed must receive at least one request");
			for (auto const& target : targets)
			{
				Assert::IsTrue(
					target == "/file.bin",
					L"a complete single-file URL seed must be requested verbatim");
			}

			auto const ranges = server.Ranges();
			Assert::IsTrue(
				std::ranges::any_of(
					ranges,
					[](std::string const& range)
					{
						return range.starts_with("bytes=");
					}),
				L"libtorrent must use HTTP byte ranges for the URL seed");
		}

		TEST_METHOD(V2MultiFileBoundaryDoesNotCoalesceAcrossFiles)
		{
			auto fixture = MakeMultiFileV2BoundaryFixture();
			RangeHttpServer server{
				std::unordered_map<std::string, std::vector<std::uint8_t>>{
					{ "/origin/v2-boundary/first.bin", fixture.first },
					{ "/origin/v2-boundary/second.bin", fixture.second }
				} };
			auto const outputRoot = fixture.root / L"download";
			DownloadMetainfoFromWebSeed(
				fixture.metainfo,
				outputRoot,
				server.Url("/origin/"));

			auto readFile = [](std::filesystem::path const& path)
			{
				std::ifstream input(path, std::ios::binary);
				return std::vector<std::uint8_t>(
					std::istreambuf_iterator<char>{ input },
					std::istreambuf_iterator<char>{});
			};
			Assert::IsTrue(
				readFile(outputRoot / L"v2-boundary" / L"first.bin")
					== fixture.first,
				L"the first unaligned v2 file must pass its piece hashes");
			Assert::IsTrue(
				readFile(outputRoot / L"v2-boundary" / L"second.bin")
					== fixture.second,
				L"the second v2 file must not be corrupted by a coalesced request from the previous file");

			auto const targets = server.Targets();
			Assert::IsTrue(
				std::ranges::find(
					targets,
					"/origin/v2-boundary/first.bin")
					!= targets.end(),
				L"the first file must be requested independently");
			Assert::IsTrue(
				std::ranges::find(
					targets,
					"/origin/v2-boundary/second.bin")
					!= targets.end(),
				L"the second file must be requested independently");
		}

		TEST_METHOD(TrailingSlashUrlSeedAppendsCanonicalFilePath)
		{
			auto fixture = MakeCanonicalFixture();
			RangeHttpServer server{ fixture.bytes };
			auto const output = DownloadFromWebSeed(
				fixture,
				server.Url("/origin/"));

			Assert::IsTrue(
				std::filesystem::is_regular_file(output),
				L"the trailing-slash fixture must still complete");

			auto const targets = server.Targets();
			Assert::IsFalse(
				targets.empty(),
				L"the web seed must receive at least one request");
			for (auto const& target : targets)
			{
				Assert::IsTrue(
					target == "/origin/OpenNet.Content.v1/content",
					L"a trailing-slash URL seed is a base URL and must append the canonical torrent file path");
			}
		}
	};
}
