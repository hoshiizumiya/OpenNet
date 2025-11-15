#pragma once

#include "Controls/HomePage/Header/HeaderCarousel.g.h"

namespace winrt::OpenNet::Controls::HomePage::Header::implementation
{
    struct HeaderCarousel : HeaderCarouselT<HeaderCarousel>
    {
        HeaderCarousel();
    };
}

namespace winrt::OpenNet::Controls::HomePage::Header::factory_implementation
{
    struct HeaderCarousel : HeaderCarouselT<HeaderCarousel, implementation::HeaderCarousel>
    {
    };
}
