#include "XamlWorkaround.h"
#include "InfoBarView.xaml.h"
#if __has_include("UI/Xaml/View/InfoBarView.g.cpp")
#include "UI/Xaml/View/InfoBarView.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	InfoBarView::InfoBarView()
	{
		auto& service = ::OpenNet::Service::Notification::InfoBarService::Instance();
		service.AttachDispatcher(DispatcherQueue());
		m_infoBars = service.InfoBars();
	}

	InfoBarView::~InfoBarView()
	{
		UnsubscribeInfoBars();
		if (m_clearTimer)
		{
			m_clearTimer.Stop();
		}
	}

	void InfoBarView::InitializeComponent()
	{
		InfoBarViewT::InitializeComponent();
		UpdateBadge();

		auto weak = get_weak();
		SubscribeInfoBars();
		SynchronizeState();
		Loaded([weak](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				self->SubscribeInfoBars();
				self->UpdateBadge();
				self->SynchronizeState();
			}
		});
		Unloaded([weak](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				self->UnsubscribeInfoBars();
			}
		});
	}

	void InfoBarView::SubscribeInfoBars()
	{
		if (m_infoBarsSubscribed)
		{
			return;
		}

		auto weak = get_weak();
		m_infoBarsChangedToken = m_infoBars.VectorChanged(
			[weak](auto const& sender, auto const& args)
		{
			if (auto self = weak.get())
			{
				self->OnInfoBarsVectorChanged(sender, args);
			}
		});
		m_infoBarsSubscribed = true;
	}

	Windows::Foundation::Collections::IObservableVector<OpenNet::Service::Notification::InfoBarOptions>	InfoBarView::InfoBars() const
	{
		return m_infoBars;
	}

	void InfoBarView::OnInfoBarClosed(InfoBar const& sender, InfoBarClosedEventArgs const&)
	{
		try
		{
			if (auto options = sender.DataContext().try_as<
				OpenNet::Service::Notification::InfoBarOptions>())
			{
				Remove(options);
			}
		}
		catch (winrt::hresult_error const&)
		{
			// The visual tree may already be disconnected while the app exits.
		}
	}

	void InfoBarView::Remove(OpenNet::Service::Notification::InfoBarOptions const& options)
	{
		::OpenNet::Service::Notification::InfoBarService::Instance().Remove(options);
	}

	void InfoBarView::OnInfoBarsVectorChanged(Windows::Foundation::Collections::IObservableVector<OpenNet::Service::Notification::InfoBarOptions> const&, Windows::Foundation::Collections::IVectorChangedEventArgs const& args)
	{
		UpdateBadge();
		auto const version = ++m_transitionVersion;
		HandleInfoBarsCollectionChangedAsync(args.CollectionChange() == Windows::Foundation::Collections::CollectionChange::ItemInserted, version);
	}

	void InfoBarView::UnsubscribeInfoBars()
	{
		if (!m_infoBarsSubscribed)
		{
			return;
		}
		m_infoBars.VectorChanged(m_infoBarsChangedToken);
		m_infoBarsSubscribed = false;
	}

	void InfoBarView::UpdateBadge()
	{
		if (NotificationCountBadge())
		{
			NotificationCountBadge().Value(
				static_cast<std::int32_t>(m_infoBars.Size()));
		}
	}

	void InfoBarView::SynchronizeState()
	{
		if (m_infoBars.Size() == 0)
		{
			++m_transitionVersion;
			VisibilityRoot().Visibility(Visibility::Collapsed);
			return;
		}
		auto const version = ++m_transitionVersion;
		HandleInfoBarsCollectionChangedAsync(true, version);
	}

	winrt::fire_and_forget InfoBarView::HandleInfoBarsCollectionChangedAsync(bool added, std::uint64_t const version)
	{
		auto strong = get_strong();

		if (m_infoBars.Size() > 0)
		{
			VisibilityRoot().Visibility(Visibility::Visible);
		}

		auto transition = InfoBarPanelTransitionHelper();
		if (added)
		{
			if (transition)
			{
				transition.Source(ShowButtonBorder());
				transition.Target(InfoBarItemsBorder());

				if (VisualTreeHelper::GetParent(ShowButtonBorder()) &&
					VisualTreeHelper::GetParent(InfoBarItemsBorder()))
				{
					try
					{
						co_await transition.StartAsync();
					}
					catch (winrt::hresult_error const&)
					{
						// The app can disconnect the visual tree while exiting.
					}
				}
			}

			// An item can be removed while the expansion transition is running.
			if (m_infoBars.Size() > 0)
			{
				co_return;
			}
		}

		if (version == m_transitionVersion && m_infoBars.Size() == 0)
		{
			if (transition)
			{
				transition.Source(InfoBarItemsBorder());
				transition.Target(ShowButtonBorder());

				if (VisualTreeHelper::GetParent(InfoBarItemsBorder()) &&
					VisualTreeHelper::GetParent(ShowButtonBorder()))
				{
					try
					{
						co_await transition.StartAsync();
					}
					catch (winrt::hresult_error const&)
					{
						// The app can disconnect the visual tree while exiting.
					}
				}
			}

			try
			{
				VisibilityRoot().Visibility(Visibility::Collapsed);
			}
			catch (winrt::hresult_error const&)
			{
				// The app can disconnect the visual tree while exiting.
			}
		}
	}

	void InfoBarView::OnClearAllButtonClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		if (m_clearTimer)
		{
			m_clearTimer.Stop();
		}

		auto weak = get_weak();
		m_clearTimer = DispatcherQueue().CreateTimer();
		m_clearTimer.IsRepeating(true);
		m_clearTimer.Interval(std::chrono::milliseconds(50));
		m_clearTimer.Tick(
			[weak](Microsoft::UI::Dispatching::DispatcherQueueTimer const& timer, IInspectable const&)
		{
			if (auto self = weak.get())
			{
				try
				{
					auto const count = self->m_infoBars.Size();
					if (count > 0)
					{
						self->Remove(self->m_infoBars.GetAt(count - 1));
					}
				}
				catch (winrt::hresult_error const&)
				{
					// The app can disconnect the visual tree while exiting.
					timer.Stop();
					self->m_clearTimer = nullptr;
					return;
				}
				if (self->m_infoBars.Size() == 0)
				{
					timer.Stop();
					self->m_clearTimer = nullptr;
				}
			}
			else
			{
				timer.Stop();
			}
		});
		m_clearTimer.Start();
	}
}
