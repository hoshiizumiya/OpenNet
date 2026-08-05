export module OpenNet.UI.Xaml.Control.DataTableColumnVisibilityHelper;

import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.XamlToolkit.Labs.WinUI;

export namespace OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper
{
	// DataRow maps its direct Children to DataColumns by index. Callers may
	// x:Load content inside a stable cell host, but must not x:Load the host
	// itself or later cells will be synchronized with the wrong column.
	void SynchronizeRow(
		winrt::XamlToolkit::Labs::WinUI::DataRow const& row,
		winrt::XamlToolkit::Labs::WinUI::DataColumn const* columns,
		unsigned int columnCount)
	{
		if (!row)
		{
			return;
		}

		auto const children = row.Children();
		auto const count = children.Size() < columnCount ? children.Size() : columnCount;
		for (unsigned int index = 0; index < count; ++index)
		{
			children.GetAt(index).Visibility(columns[index].Visibility());
		}

		row.InvalidateMeasure();
	}

	void SynchronizeRealizedRows(
		winrt::Microsoft::UI::Xaml::DependencyObject const& root,
		winrt::XamlToolkit::Labs::WinUI::DataColumn const* columns,
		unsigned int columnCount)
	{
		if (!root)
		{
			return;
		}

		if (auto const row = root.try_as<winrt::XamlToolkit::Labs::WinUI::DataRow>())
		{
			SynchronizeRow(row, columns, columnCount);
			return;
		}

		auto const childCount =
			winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
		for (int index = 0; index < childCount; ++index)
		{
			SynchronizeRealizedRows(
				winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChild(root, index),
				columns,
				columnCount);
		}
	}
}
