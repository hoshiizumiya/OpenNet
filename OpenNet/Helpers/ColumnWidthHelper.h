/*
 * PROJECT:   OpenNet
 * FILE:      Helpers/ColumnWidthHelper.h
 * PURPOSE:   Save / restore DataColumn widths via AppSettingsDatabase.
 *
 * LICENSE:   Attribution-NonCommercial-ShareAlike 4.0 International
 */

#pragma once

#include "Core/AppSettingsDatabase.h"
#include <winrt/Microsoft.UI.Xaml.h>

namespace OpenNet::Helpers
{
	inline void SaveColumnWidth(std::string const& key, double width)
	{
		if (width > 0)
			::OpenNet::Core::AppSettingsDatabase::Instance().SetDouble(
				::OpenNet::Core::AppSettingsDatabase::CAT_COLUMN_WIDTH, key, width);
	}

	inline double GetColumnWidth(std::string const& key, double defaultWidth = 0.0)
	{
		return ::OpenNet::Core::AppSettingsDatabase::Instance().GetDouble(
			::OpenNet::Core::AppSettingsDatabase::CAT_COLUMN_WIDTH, key).value_or(defaultWidth);
	}

	/// Restore a column's DesiredWidth from persisted pixel value.
	/// Does nothing if no saved width exists.
	template <typename TColumn>
	inline void RestoreColumn(TColumn const& col, std::string const& key)
	{
		double w = GetColumnWidth(key);
		if (w > 0)
		{
			col.Width(w);
		}
	}
}
