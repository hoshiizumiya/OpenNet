module;
#include <sentry.h>

export module OpenNet.Core.ExceptionService.CapturedException;

import winrt_base;
import std;

export namespace OpenNet::Core::ExceptionService
{
	class CapturedException
	{
	public:
		CapturedException(sentry_uuid_t sentryId, const std::exception& ex)
			: SentryId(sentryId), stdException(ex)
		{
		}

		CapturedException(sentry_uuid_t sentryId, const winrt::hresult& ex)
			: SentryId(sentryId), hresultException(ex)
		{
		}
	private:
		const sentry_uuid_t SentryId;
		const std::exception stdException;
		const winrt::hresult hresultException;
	};
}