export module OpenNet.Core.Content.ContentDirectorySyncService;

import std;

export namespace OpenNet::Core::Content
{
    class ContentDirectorySyncService
    {
    public:
        static ContentDirectorySyncService& Instance();

        void Start();
        void Stop();
        void MarkDirty();

    private:
        ContentDirectorySyncService() = default;
        ~ContentDirectorySyncService();

        ContentDirectorySyncService(ContentDirectorySyncService const&) = delete;
        ContentDirectorySyncService& operator=(ContentDirectorySyncService const&) = delete;

        void WorkerLoop();
        static std::string GetOrCreateNodeId();

        std::mutex m_mutex;
        std::condition_variable m_condition;
        std::thread m_worker;
        std::stop_source m_stopSource;
        bool m_started{};
        bool m_stopping{};
        std::uint64_t m_revision{ 1 };
        std::uint64_t m_syncedRevision{};
        std::uint64_t m_announcedResourceRevision{};
    };
}
