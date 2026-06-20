module;
#include <Windows.h>

module OpenNet.Core.Utils.Message;

using namespace winrt;
using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;

namespace OpenNet::Core::Utils::Message
{
	void ShowMessageBox(const wchar_t* message, Severity level)
	{
		hstring text = ResourceGetString(message);
		hstring caption;
		switch (level)
		{
			case Info:
				caption = ResourceGetString(L"MBInfo");
				MessageBoxW(0, text.c_str(), caption.c_str(), MB_OK | MB_ICONINFORMATION);
				break;
			case Warn:
				caption = ResourceGetString(L"MBWarn");
				MessageBoxW(0, text.c_str(), caption.c_str(), MB_OK | MB_ICONWARNING);
				break;
			case Error:
				caption = ResourceGetString(L"MBError");
				MessageBoxW(0, text.c_str(), caption.c_str(), MB_OK | MB_ICONERROR);
				break;
			default:
				break;
		}

	}

	void ShowErrorMessage(const wchar_t* title, const wchar_t* message)
	{
		if ((title) == nullptr)
		{
			// TODO
		}

		// Show message box
		int result = MessageBoxW(
			nullptr,           // No parent window
			message,           // Message text
			title,             // Title
			MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST
		);

		// MessageBoxW returns IDOK (1) when user clicks OK button
		// We always return S_OK because message box was shown successfully
	}
}


hstring ResourceGetString(const wchar_t* resourceId)
{
	ResourceLoader loader;
	return loader.GetString(resourceId);
}

