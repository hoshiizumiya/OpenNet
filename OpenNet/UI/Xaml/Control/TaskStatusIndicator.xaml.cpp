#include "XamlWorkaround.h"
#include "TaskStatusIndicator.xaml.h"
#if __has_include("UI/Xaml/Control/TaskStatusIndicator.g.cpp")
#include "UI/Xaml/Control/TaskStatusIndicator.g.cpp"
#endif

import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Automation;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.Windows.ApplicationModel.Resources;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Automation;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceLoader;

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	DependencyProperty TaskStatusIndicator::s_stateProperty =
		DependencyProperty::Register(
			L"State",
			xaml_typename<std::int32_t>(),
			xaml_typename<class_type>(),
			PropertyMetadata{
				box_value(std::int32_t{}),
				PropertyChangedCallback{ &TaskStatusIndicator::OnStateChanged } });

	TaskStatusIndicator::TaskStatusIndicator()
	{
		InitializeComponent();
		UpdateVisualState();
	}

	std::int32_t TaskStatusIndicator::State() const
	{
		return unbox_value_or<std::int32_t>(GetValue(StateProperty()), 0);
	}

	void TaskStatusIndicator::State(std::int32_t value)
	{
		SetValue(StateProperty(), box_value(value));
	}

	DependencyProperty TaskStatusIndicator::StateProperty()
	{
		return s_stateProperty;
	}

	void TaskStatusIndicator::OnStateChanged(
		DependencyObject const& sender,
		DependencyPropertyChangedEventArgs const&)
	{
		if (auto self = sender.try_as<
			winrt::OpenNet::UI::Xaml::Control::TaskStatusIndicator>())
		{
			get_self<TaskStatusIndicator>(self)->UpdateVisualState();
		}
	}

	void TaskStatusIndicator::UpdateVisualState()
	{
		auto stateName = L"Pending";
		auto resourceKey = L"TaskStatusPending";
		switch (State())
		{
			case 1: // DownloadTaskState::Downloading
				stateName = L"Downloading";
				resourceKey = L"TaskStatusDownloading";
				break;
			case 2: // DownloadTaskState::Seeding
				stateName = L"Seeding";
				resourceKey = L"TaskStatusSeeding";
				break;
			case 3: // DownloadTaskState::Paused
				stateName = L"Paused";
				resourceKey = L"TaskStatusPaused";
				break;
			case 4: // DownloadTaskState::Completed
				stateName = L"Completed";
				resourceKey = L"TaskStatusCompleted";
				break;
			case 5: // DownloadTaskState::Failed
				stateName = L"Failed";
				resourceKey = L"TaskStatusFailed";
				break;
			default:
				break;
		}

		VisualStateManager::GoToState(*this, stateName, true);
		winrt::hstring label{ stateName };
		try
		{
			if (auto localized = ResourceLoader{}.GetString(resourceKey);
				!localized.empty())
			{
				label = localized;
			}
		}
		catch (...)
		{
		}

		ToolTipService::SetToolTip(StatusSurface(), box_value(label));
		AutomationProperties::SetName(StatusSurface(), label);
	}
}
