module;

#include <wil/result.h>

export module OpenNet.Core.ExceptionService.OpenNetException;

import std;

export namespace OpenNet::Core::ExceptionService
{
	class OpenNetException : public wil::ResultException
	{
	public:
		OpenNetException(HRESULT const& hr) : wil::ResultException(hr)
		{

		}

		OpenNetException(wil::ResultException const& ex) : wil::ResultException(ex)
		{
		}

		OpenNetException(HRESULT hr, std::wstring message) : wil::ResultException(hr), m_message(std::move(message))
		{
		}

		std::wstring Message() const noexcept
		{
			return m_message;
		}

	private:
		std::wstring m_message;
	};
}
