/*
 * PROJECT:   OpenNet
 * FILE:      Core/Aria2/Aria2Engine.h
 * PURPOSE:   Aria2 engine management (RPC client + local process)
 *            (migrated from NanaGetCore.h)
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>

#include "Core/Aria2/Aria2Models.h"

namespace OpenNet::Core::Aria2
{
    // ---------------------------------------------------------------
    //  Utility functions
    // ---------------------------------------------------------------

    std::filesystem::path GetApplicationFolderPath();
    std::filesystem::path GetSettingsFolderPath();

    winrt::hstring CreateGuidString();
    winrt::hstring ConvertSecondsToTimeString(std::uint64_t Seconds);

    bool FindSubString(
        winrt::hstring const &SourceString,
        winrt::hstring const &SubString,
        bool IgnoreCase);

    // ---------------------------------------------------------------
    //  Aria2Instance  – RPC client for any Aria2 endpoint
    // ---------------------------------------------------------------

    class Aria2Instance
    {
    public:
        Aria2Instance(
            winrt::Windows::Foundation::Uri const &ServerUri,
            std::string const &ServerToken);

        virtual ~Aria2Instance();

        winrt::Windows::Foundation::Uri ServerUri();
        std::string ServerToken();

        void Shutdown(bool Force = false);
        void PauseAll(bool Force = false);
        void ResumeAll();
        void ClearList();

        void Pause(std::string const &Gid, bool Force = false);
        void Resume(std::string const &Gid);
        void Cancel(std::string const &Gid, bool Force = false);
        void Remove(std::string const &Gid);

        std::string AddUri(std::string const &Source);
        std::string AddUriWithOptions(
            std::string const &Source,
            std::string const &Dir = {},
            std::string const &OutFileName = {});

        std::mutex &InstanceLock();

        std::uint64_t TotalDownloadSpeed();
        std::uint64_t TotalUploadSpeed();
        std::size_t NumActive();
        std::size_t NumWaiting();
        std::size_t NumStopped();

        void RefreshInformation();

        DownloadInformation GetTaskInformation(std::string const &Gid);
        std::vector<std::string> GetTaskList();

        std::string SimplePost(std::string const &Content);
        std::string SimpleJsonRpcCall(
            std::string const &MethodName,
            std::string const &Parameters);

    protected:
        Aria2Instance();
        void UpdateInstance(
            winrt::Windows::Foundation::Uri const &ServerUri,
            std::string const &ServerToken);

    private:
        winrt::Windows::Foundation::Uri m_ServerUri{nullptr};
        std::string m_ServerToken;
        winrt::Windows::Web::Http::HttpClient m_HttpClient;

        std::mutex m_InstanceLock;
        GlobalStatusInformation m_GlobalStatus;
    };

    // ---------------------------------------------------------------
    //  LocalAria2Instance  – manages a child Aria2 process
    // ---------------------------------------------------------------

    class LocalAria2Instance : public Aria2Instance
    {
    public:
        LocalAria2Instance();
        ~LocalAria2Instance();

        void Restart();
        bool Available() const;

        /// Graceful shutdown: RPC Shutdown → wait up to 30s → ForceTerminate.
        void Terminate();

        /// Immediate process kill. Safe to call from any thread (no RPC/.get()).
        void ForceTerminate();

    private:
        std::uint16_t PickUnusedTcpPort();
        void Startup();

    private:
        bool m_Available = false;
        winrt::handle m_ProcessHandle;
        winrt::handle m_JobObjectHandle;
    };
}
