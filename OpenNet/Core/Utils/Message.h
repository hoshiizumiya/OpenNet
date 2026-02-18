#pragma once

namespace Core::Utils::Message
{
	enum Severity
	{
		Info = 0,
		Warn,
		Error
	};

	void ShowMessageBox(const wchar_t* message, Severity level);

}
winrt::hstring ResourceGetString(const wchar_t* resourceId);
