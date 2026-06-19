export module OpenNet.Core.Utils.Message;

import winrt_base;
import winrt.Microsoft.Windows.ApplicationModel.Resources;

export namespace Core::Utils::Message
{
	enum Severity
	{
		Info = 0,
		Warn,
		Error
	};

	void ShowMessageBox(const wchar_t* message, Severity level);

}

export winrt::hstring ResourceGetString(const wchar_t* resourceId);
