export module OpenNet.UI.Xaml.Control.DataTableSortHelper;

import std;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Automation;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Controls.AnimatedVisuals;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.Windows.ApplicationModel.Resources;

export namespace OpenNet::UI::Xaml::Control
{
	// Keeps the visual and accessibility state of sortable DataTable headers
	// consistent without introducing textual standard-library headers into
	// translation units that consume the C++23 std module.
	struct DataTableSortHelper final
	{
		static void UpdateHeader(
			winrt::Microsoft::UI::Xaml::Controls::Button const& button,
			winrt::hstring const& activeColumn,
			int direction)
		{
			using namespace winrt;
			using namespace Microsoft::UI::Xaml;
			using namespace Microsoft::UI::Xaml::Automation;
			using namespace Microsoft::UI::Xaml::Controls;
			using namespace Microsoft::UI::Xaml::Controls::AnimatedVisuals;

			if (!button || !button.Tag())
			{
				return;
			}

			auto const column = unbox_value_or<hstring>(button.Tag(), L"");
			auto const nextDirection = column == activeColumn ? direction : 0;
			auto [label, indicator] = EnsureVisuals(button);
			if (!label || !indicator)
			{
				return;
			}

			auto const previousDirection =
				unbox_value_or<std::int32_t>(indicator.Tag(), 0);
			indicator.Tag(box_value(static_cast<std::int32_t>(nextDirection)));

			if (nextDirection == 0)
			{
				indicator.Visibility(Visibility::Collapsed);
			}
			else
			{
				indicator.Visibility(Visibility::Visible);
				FontIconSource fallback;
				fallback.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons" });
				fallback.Glyph(nextDirection == 1 ? L"\uE70D" : L"\uE70E");
				indicator.FallbackIconSource(fallback);

				if (nextDirection == 1)
				{
					indicator.Source(AnimatedChevronUpDownSmallVisualSource{});
				}
				else
				{
					indicator.Source(AnimatedChevronDownSmallVisualSource{});
				}

				AnimatedIcon::SetState(indicator, L"Normal");
				if (previousDirection != nextDirection)
				{
					auto weakIndicator = make_weak(indicator);
					button.DispatcherQueue().TryEnqueue([weakIndicator]()
					{
						if (auto current = weakIndicator.get())
						{
							AnimatedIcon::SetState(current, L"PointerOver");
						}
					});
				}
			}

			auto const hint = SortHint(nextDirection);
			ToolTipService::SetToolTip(button, box_value(hint));
			AutomationProperties::SetHelpText(button, hint);
			std::wstring automationName{ label.Text() };
			automationName.append(L" — ");
			automationName.append(hint.c_str(), hint.size());
			AutomationProperties::SetName(button, hstring{ automationName });
		}

	private:
		static std::pair<
			winrt::Microsoft::UI::Xaml::Controls::TextBlock,
			winrt::Microsoft::UI::Xaml::Controls::AnimatedIcon>
			EnsureVisuals(
				winrt::Microsoft::UI::Xaml::Controls::Button const& button)
		{
			using namespace winrt;
			using namespace Microsoft::UI::Xaml;
			using namespace Microsoft::UI::Xaml::Controls;

			if (auto grid = button.Content().try_as<Grid>())
			{
				TextBlock label{ nullptr };
				AnimatedIcon indicator{ nullptr };
				for (auto const& child : grid.Children())
				{
					if (!label)
					{
						label = child.try_as<TextBlock>();
					}
					if (auto icon = child.try_as<AnimatedIcon>();
						icon && icon.Name() == L"DataTableSortIndicator")
					{
						indicator = icon;
					}
				}
				if (label && indicator)
				{
					return { label, indicator };
				}
			}

			auto label = button.Content().try_as<TextBlock>();
			if (!label)
			{
				return { nullptr, nullptr };
			}

			button.Content(nullptr);
			Grid grid;
			ColumnDefinition textColumn;
			textColumn.Width(GridLength{ 1.0, GridUnitType::Star });
			ColumnDefinition indicatorColumn;
			indicatorColumn.Width(GridLength{ 1.0, GridUnitType::Auto });
			grid.ColumnDefinitions().Append(textColumn);
			grid.ColumnDefinitions().Append(indicatorColumn);

			Grid::SetColumn(label, 0);
			grid.Children().Append(label);

			AnimatedIcon indicator;
			indicator.Name(L"DataTableSortIndicator");
			indicator.Width(16.0);
			indicator.Height(16.0);
			indicator.Margin(Thickness{ 4.0, 0.0, 0.0, 0.0 });
			indicator.HorizontalAlignment(HorizontalAlignment::Center);
			indicator.VerticalAlignment(VerticalAlignment::Center);
			indicator.Visibility(Visibility::Collapsed);
			Grid::SetColumn(indicator, 1);
			grid.Children().Append(indicator);

			auto weakIndicator = make_weak(indicator);
			button.PointerEntered(
				[weakIndicator](winrt::Windows::Foundation::IInspectable const&,
								Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
			{
				if (auto current = weakIndicator.get();
					current && current.Visibility() == Visibility::Visible)
				{
					AnimatedIcon::SetState(current, L"PointerOver");
				}
			});
			button.PointerExited(
				[weakIndicator](winrt::Windows::Foundation::IInspectable const&,
								Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
			{
				if (auto current = weakIndicator.get(); current)
				{
					AnimatedIcon::SetState(current, L"Normal");
				}
			});

			button.Content(grid);
			return { label, indicator };
		}

		static winrt::hstring SortHint(int direction)
		{
			using winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceLoader;
			auto const key = direction == 1
				? L"DataTableSortAscendingHint"
				: direction == 2
				? L"DataTableSortDescendingHint"
				: L"DataTableSortNoneHint";
			auto const fallback = direction == 1
				? L"Sorted ascending. Activate to sort descending."
				: direction == 2
				? L"Sorted descending. Activate to clear sorting."
				: L"Not sorted. Activate to sort ascending.";
			try
			{
				auto value = ResourceLoader{}.GetString(key);
				return value.empty() ? winrt::hstring{ fallback } : value;
			}
			catch (...)
			{
				return winrt::hstring{ fallback };
			}
		}
	};
}
