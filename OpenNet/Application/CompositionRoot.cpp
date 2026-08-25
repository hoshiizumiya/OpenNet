module;

#include "WindowsPlatform.h"
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "ThirdParty/boost/di.hpp"
#pragma pop_macro("max")
#pragma pop_macro("min")

module OpenNet.Application.CompositionRoot;

namespace OpenNet::Application
{
	namespace di = boost::di;

	struct CompositionRoot::Impl
	{
		std::shared_ptr<Tasks::ITaskCommandService> taskCommandService;
	};

	CompositionRoot& CompositionRoot::Instance()
	{
		static CompositionRoot root;
		return root;
	}

	CompositionRoot::CompositionRoot() : m_impl(std::make_unique<Impl>())
	{
		auto service = Tasks::CreateTaskCommandService();
		auto injector = di::make_injector(
			di::bind<Tasks::ITaskCommandService>.to(service));
		m_impl->taskCommandService =
			injector.create<std::shared_ptr<Tasks::ITaskCommandService>>();
	}

	std::shared_ptr<Tasks::ITaskCommandService> CompositionRoot::TaskCommandService() const
	{
		return m_impl->taskCommandService;
	}
}
