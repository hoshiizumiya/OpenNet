export module OpenNet.Application.Tasks.TaskCommandService;

import std;
import winrt.Windows.Foundation;

export namespace OpenNet::Application::Tasks
{
	enum class TaskKind
	{
		BitTorrent,
		Http,
	};

	struct TaskOperation
	{
		TaskKind kind{};
		std::string taskId;
		std::string gid;
		bool deleteDownloadedFiles{};
	};

	struct ITaskCommandService
	{
		virtual ~ITaskCommandService() = default;
		virtual winrt::Windows::Foundation::IAsyncAction StartAsync(TaskOperation operation) = 0;
		virtual winrt::Windows::Foundation::IAsyncAction PauseAsync(TaskOperation operation) = 0;
		virtual winrt::Windows::Foundation::IAsyncAction DeleteAsync(TaskOperation operation) = 0;
	};

	std::shared_ptr<ITaskCommandService> CreateTaskCommandService();
}
