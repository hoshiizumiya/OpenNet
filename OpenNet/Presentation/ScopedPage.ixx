export module OpenNet.Presentation.ScopedPage;

import std;

export namespace OpenNet::Presentation
{
	template <typename TViewModel>
	class ScopedPage
	{
	protected:
		void InitializeScopedViewModel(
			TViewModel viewModel,
			std::function<void()> activate,
			std::function<void()> deactivate)
		{
			m_viewModel = std::move(viewModel);
			m_activate = std::move(activate);
			m_deactivate = std::move(deactivate);
		}

		void ActivateScopedViewModel()
		{
			if (std::exchange(m_isActive, true)) return;
			if (m_activate) m_activate();
		}

		void DeactivateScopedViewModel()
		{
			if (!std::exchange(m_isActive, false)) return;
			if (m_deactivate) m_deactivate();
		}

		TViewModel m_viewModel{ nullptr };

	private:
		std::function<void()> m_activate;
		std::function<void()> m_deactivate;
		bool m_isActive{};
	};
}
