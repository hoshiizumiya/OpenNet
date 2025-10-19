#include "pch.h"
#include "Folder.h"
#if __has_include("Pages/SettingsPages/Folder.g.cpp")
#include "Pages/SettingsPages/Folder.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	Folder::Folder()
	{
	}

	winrt::hstring Folder::Name()
	{
		return m_name;
	}

	void Folder::Name(winrt::hstring const& value)
	{
		if (m_name == value) return;
		m_name = value;

		m_propertyChanged(*this, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Name" });
	}

	// must implement INotifyPropertyChanged to avoid Xaml warnings/errorsd();
	winrt::event_token Folder::PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
	{
		return m_propertyChanged.add(handler);
	}

	void Folder::PropertyChanged(winrt::event_token const& token) noexcept
	{
		m_propertyChanged.remove(token);
	}
}
