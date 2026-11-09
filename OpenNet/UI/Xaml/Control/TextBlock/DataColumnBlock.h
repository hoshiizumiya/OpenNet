#pragma once

#include "UI/Xaml/Control/TextBlock/DataColumnBlock.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::TextBlock::implementation
{
	struct DataColumnBlock : DataColumnBlockT<DataColumnBlock>
	{
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::TextBlock::factory_implementation
{
	struct DataColumnBlock : DataColumnBlockT<DataColumnBlock, implementation::DataColumnBlock>
	{
	};
}
