#include "XamlWorkaround.h"
#include "CultureOptions.h"
#include "Service/CultureOptions.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::Service::implementation
{
	hstring CultureOptions::CultureInfo()
	{
		throw hresult_not_implemented();
	}
	void CultureOptions::CultureInfo(hstring const& value)
	{
		throw hresult_not_implemented();
	}

}
