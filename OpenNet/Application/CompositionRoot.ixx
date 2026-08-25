export module OpenNet.Application.CompositionRoot;

import std;
import OpenNet.Application.Tasks.TaskCommandService;

export namespace OpenNet::Application
{
	class CompositionRoot
	{
	public:
		static CompositionRoot& Instance();
		std::shared_ptr<Tasks::ITaskCommandService> TaskCommandService() const;

	private:
		CompositionRoot();
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
