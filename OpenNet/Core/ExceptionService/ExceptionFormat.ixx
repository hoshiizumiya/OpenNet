module;
#include <sentry.h>

export module OpenNet.Core.ExceptionService.ExceptionFormat;

import winrt_base;

export namespace OpenNet::Core::ExceptionService::ExceptionFormat
{
	inline winrt::hstring ToHString(sentry_uuid_t const& uuid)
	{
		if (sentry_uuid_is_nil(&uuid))
		{
			return L"unavailable";
		}

		char value[37]{};
		sentry_uuid_as_string(&uuid, value);
		return winrt::to_hstring(value);
	}

	inline sentry_uuid_t ToSentryUuid(winrt::hstring const& value)
	{
		auto const utf8Value = winrt::to_string(value);
		return sentry_uuid_from_string(utf8Value.c_str());
	}
}
