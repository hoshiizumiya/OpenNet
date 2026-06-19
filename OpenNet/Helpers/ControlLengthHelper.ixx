/*
 * PROJECT:   OpenNet
 * FILE:      Helpers/ControlLengthHelper.ixx
 * PURPOSE:   Save / restore control widths via AppSettingsDatabase.
 *
 * LICENSE:   Attribution-NonCommercial-ShareAlike 4.0 International
 */
export module OpenNet.Helpers.ControlLengthHelper;

import OpenNet.Core.AppSettingsDatabase;
import winrt.Microsoft.UI.Xaml;

export namespace OpenNet::Helpers
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