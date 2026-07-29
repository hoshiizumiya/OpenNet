module;
#include <sentry.h>

export module OpenNet.Core.ExceptionService.CapturedException;

import OpenNet.Core.ExceptionService.ExceptionFormat;
import winrt_base;

export namespace OpenNet::Core::ExceptionService
{
	class CapturedException
	{
	public:
		CapturedException(
			sentry_uuid_t const& sentryId,
			winrt::hstring const& exception)
			: m_sentryId(ExceptionFormat::ToHString(sentryId)),
			m_exception(exception)
		{
		}

		winrt::hstring SentryId() const noexcept
		{
			return m_sentryId;
		}

		winrt::hstring Exception() const noexcept
		{
			return m_exception;
		}

	private:
		winrt::hstring m_sentryId;
		winrt::hstring m_exception;
	};
}
