module;
#include <sentry.h>

export module OpenNet.Core.ExceptionService.ExceptionFormat;

import winrt_base;

export namespace OpenNet::Core::ExceptionService::ExceptionFormat
{
	inline sentry_uuid_t ToSentryUuid(winrt::guid const& guid) noexcept
	{
		sentry_uuid_t uuid{};

		std::memcpy(
			uuid.bytes,
			&guid,
			sizeof(uuid.bytes));

		return uuid;
	}
}