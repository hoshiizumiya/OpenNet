export module OpenNet.Core.Utils.Message;

import winrt_base;
import winrt.Microsoft.Windows.ApplicationModel.Resources;

export namespace OpenNet::Core::Utils::Message
{
	enum Severity
	{
		Info = 0,
		Warn,
		Error
	};

	void ShowMessageBox(const wchar_t* message, Severity level);
	void ShowErrorMessage(const wchar_t* title, const wchar_t* message);

}

export winrt::hstring ResourceGetString(const wchar_t* resourceId);
