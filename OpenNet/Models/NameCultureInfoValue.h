#pragma once

import winrt.OpenNet.Models;
#include "Models/NameCultureInfoValue.g.h"

namespace winrt::OpenNet::Models::implementation
{
	struct NameCultureInfoValue : NameCultureInfoValueT<NameCultureInfoValue>
	{
		NameCultureInfoValue();

		winrt::hstring Name();
		void Name(winrt::hstring const& value);
		winrt::hstring Value();
		void Value(winrt::hstring const& value);
		bool IsMaintainedByMSTRDI() const;
		void IsMaintainedByMSTRDI(bool value);
		bool IsMaintainedByCrowdin() const;
		void IsMaintainedByCrowdin(bool value);

	private:
		winrt::hstring m_name;
		winrt::hstring m_value;
		bool m_isMaintainedByMSTRDI{ false };
		bool m_isMaintainedByCrowdin{ false };
	};
}

namespace winrt::OpenNet::Models::factory_implementation
{
	struct NameCultureInfoValue : NameCultureInfoValueT<NameCultureInfoValue, implementation::NameCultureInfoValue>
	{
	};
}
