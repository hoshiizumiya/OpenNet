#include "pch.h"
#include "Message.h"
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>

using namespace winrt;
using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;

namespace Core::Utils::Message
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
}


hstring ResourceGetString(const wchar_t* resourceId)
{
	ResourceLoader loader;
	return loader.GetString(resourceId);
}

