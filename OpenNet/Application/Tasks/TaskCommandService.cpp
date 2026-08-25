module;

#include "Core/DataGraph/SpeedGraphDatabase.h"

module OpenNet.Application.Tasks.TaskCommandService;

import OpenNet.Core.DownloadManager;
import OpenNet.Core.HttpStateManager;
import OpenNet.Core.P2PManager;

namespace OpenNet::Application::Tasks
{
	namespace
	{
		class TaskCommandService final : public ITaskCommandService
		{
		public:
			winrt::Windows::Foundation::IAsyncAction StartAsync(TaskOperation operation) override
			{
				co_await winrt::resume_background();
				if (operation.kind == TaskKind::BitTorrent)
				{
					auto& manager = ::OpenNet::Core::P2PManager::Instance();
					co_await manager.EnsureTorrentCoreInitializedAsync();
					if (auto* core = manager.TorrentCore())
					{
						if (!operation.taskId.empty()) core->ResumeTorrent(operation.taskId);
						else core->Start();
					}
					co_return;
				}

				if (!operation.gid.empty())
				{
					::OpenNet::Core::DownloadManager::Instance().ResumeHttpDownload(operation.gid);
				}
			}

			winrt::Windows::Foundation::IAsyncAction PauseAsync(TaskOperation operation) override
			{
				co_await winrt::resume_background();
				if (operation.kind == TaskKind::BitTorrent)
				{
					if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
						core && !operation.taskId.empty())
					{
						core->PauseTorrent(operation.taskId);
					}
					co_return;
				}

				if (!operation.gid.empty())
				{
					::OpenNet::Core::DownloadManager::Instance().PauseHttpDownload(operation.gid);
				}
			}

			winrt::Windows::Foundation::IAsyncAction DeleteAsync(TaskOperation operation) override
			{
				co_await winrt::resume_background();
				if (operation.kind == TaskKind::BitTorrent)
				{
					auto& manager = ::OpenNet::Core::P2PManager::Instance();
					if (auto* core = manager.TorrentCore(); core && !operation.taskId.empty())
					{
						core->RemoveTorrent(operation.taskId, operation.deleteDownloadedFiles);
					}
					if (auto* state = manager.StateManager(); state && !operation.taskId.empty())
					{
						state->DeleteTask(operation.taskId);
					}
					if (!operation.taskId.empty())
					{
						::OpenNet::Core::SpeedGraphDatabase::Instance().DeleteTask(operation.taskId);
					}
					co_return;
				}

				auto& manager = ::OpenNet::Core::DownloadManager::Instance();
				auto recordId = operation.taskId;
				if (recordId.empty() && !operation.gid.empty())
				{
					recordId = manager.GetRecordIdForGid(operation.gid);
				}
				if (!operation.gid.empty())
				{
					manager.DeleteHttpDownload(operation.gid, operation.deleteDownloadedFiles);
				}
				if (!recordId.empty())
				{
					::OpenNet::Core::HttpStateManager::Instance().DeleteRecord(recordId);
					::OpenNet::Core::SpeedGraphDatabase::Instance().DeleteTask(recordId);
				}
			}
		};
	}

	std::shared_ptr<ITaskCommandService> CreateTaskCommandService()
	{
		return std::make_shared<TaskCommandService>();
	}
}
