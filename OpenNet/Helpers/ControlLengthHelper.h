/*
 * PROJECT:   OpenNet
 * FILE:      Helpers/ControlLengthHelper.h
 * PURPOSE:   Save / restore control widths via AppSettingsDatabase.
 *
 * LICENSE:   Attribution-NonCommercial-ShareAlike 4.0 International
 */

#pragma once

#include "Core/AppSettingsDatabase.h"
#include <winrt/Microsoft.UI.Xaml.h>

namespace OpenNet::Helpers
{
	inline void SaveControlHeight(std::string const& key, double height)
	{
		if (height > 0)
			::OpenNet::Core::AppSettingsDatabase::Instance().SetDouble(
				::OpenNet::Core::AppSettingsDatabase::CAT_CONTROL_HEIGHT, key, height);
	}

	inline double GetControlHeight(std::string const& key, double defaultHeight = 0.0)
	{
		return ::OpenNet::Core::AppSettingsDatabase::Instance().GetDouble(
			::OpenNet::Core::AppSettingsDatabase::CAT_CONTROL_HEIGHT, key).value_or(defaultHeight);
	}

	/// Restore a control's Height from persisted pixel value.
	/// Does nothing if no saved height exists.
	template <typename TControl>
	inline void RestoreControlHeight(TControl const& control, std::string const& key)
	{
		double h = GetControlHeight(key);
		if (h > 0)
		{
			control.Height(h);
		}
	}
}