#include "pch.h"
#include "NameCultureInfoValue.h"
#if __has_include("Models/NameCultureInfoValue.g.cpp")
#include "Models/NameCultureInfoValue.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::Models::implementation
{
	NameCultureInfoValue::NameCultureInfoValue()
	{
		m_isMaintainedByMSTRDI = false;
		m_isMaintainedByCrowdin = false;
	}

	winrt::hstring NameCultureInfoValue::Value()
	{
		return m_value;
	}

	void NameCultureInfoValue::Value(winrt::hstring const& value)
	{
		m_value = value;
	}

	bool NameCultureInfoValue::IsMaintainedByMSTRDI() const
	{
		return m_isMaintainedByMSTRDI;
	}

	void NameCultureInfoValue::IsMaintainedByMSTRDI(bool value)
	{
		m_isMaintainedByMSTRDI = value;
	}

	bool NameCultureInfoValue::IsMaintainedByCrowdin() const
	{
		return m_isMaintainedByCrowdin;
	}

	void NameCultureInfoValue::IsMaintainedByCrowdin(bool value)
	{
		m_isMaintainedByCrowdin = value;
	}
}
