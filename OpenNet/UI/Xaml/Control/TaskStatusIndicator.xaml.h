#pragma once

#include "UI/Xaml/Control/TaskStatusIndicator.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	struct TaskStatusIndicator : TaskStatusIndicatorT<TaskStatusIndicator>
	{
		TaskStatusIndicator();

		std::int32_t State() const;
		void State(std::int32_t value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty StateProperty();

	private:
		static void OnStateChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& sender, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		void UpdateVisualState();

		static winrt::Microsoft::UI::Xaml::DependencyProperty s_stateProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::factory_implementation
{
	struct TaskStatusIndicator : TaskStatusIndicatorT<TaskStatusIndicator, implementation::TaskStatusIndicator>
	{
	};
}
