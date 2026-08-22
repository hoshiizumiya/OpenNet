module;
#include <Windows.h>
#include <DbgHelp.h>
#include <RestrictedErrorInfo.h>
#include <sentry.h>

#pragma comment(lib, "Dbghelp.lib")

export module OpenNet.Core.ExceptionService.ExceptionHandling;

import OpenNet.Core.ExceptionService.CapturedException;
import OpenNet.XamlApplicationLifetime;
import winrt_base;
import winrt.Windows.Foundation;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml;
import winrt.OpenNet.UI.Xaml.View.Windows;
import std;

export namespace OpenNet::Core::ExceptionService
{
	class ExceptionHandling
	{
	public:
		static void OnAppUnhandledException(winrt::Windows::Foundation::IInspectable /*sender*/, winrt::Microsoft::UI::Xaml::UnhandledExceptionEventArgs e)
		{
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
			if (IsDebuggerPresent())
			{
				auto const errorMessage = e.Message();
				OutputDebugStringW((
					L"UnhandledException: "
					+ std::wstring(errorMessage.c_str())
					+ L"\r\n").c_str());
				__debugbreak();
			}
#endif
			// WinUI returns control to the message pump only when this is true.
			e.Handled(true);

			// A second exception means that the fatal-error UI itself (or the
			// already-corrupted UI thread) failed. Do not recurse indefinitely.
			if (s_isHandlingException.exchange(true))
			{
				OutputDebugStringW(
					L"A second unhandled exception occurred while displaying "
					L"the exception window.\r\n");
				sentry_flush(2000);
				ExitProcess(EXIT_FAILURE);
			}

			OpenNet::XamlApplicationLifetime::Exiting = true;

			auto stackTrace = CaptureExceptionStackTrace();
			HRESULT const errorCode = e.Exception();
			std::wstring const message{ e.Message().c_str() };
			winrt::hstring const details{
				std::format(
					L"{}\r\n\r\nHRESULT: 0x{:08X}\r\n\r\n{}",
					message,
					static_cast<uint32_t>(errorCode),
					stackTrace.Formatted)
			};
			CapturedException const capturedException{
				CaptureException(
					errorCode,
					e.Message(),
					stackTrace.Frames,
					details),
				details
			};

			// Creating another XAML window from inside UnhandledException can
			// re-enter WinUI. Queue it so the failing callback can unwind first.
			auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
			if (!dispatcher || !dispatcher.TryEnqueue([capturedException]()
			{
				try
				{
					winrt::OpenNet::UI::Xaml::View::Windows::
						ExceptionWindow::Show(
							capturedException.SentryId(),
							capturedException.Exception());
				}
				catch (...)
				{
					OutputDebugStringW(
						L"Failed to display the exception window.\r\n");
					sentry_flush(2000);
					ExitProcess(EXIT_FAILURE);
				}
			}))
			{
				OutputDebugStringW(
					L"Failed to enqueue the exception window.\r\n");
				sentry_flush(2000);
				ExitProcess(EXIT_FAILURE);
			}
		}

	private:
		struct ExceptionStackTrace
		{
			std::vector<void*> Frames;
			std::wstring Formatted;
		};

		static std::vector<void*> TryCaptureLanguageExceptionStack() noexcept
		{
			try
			{
				// The WinRT error object is stored in thread-local COM state.
				// Read it without consuming it so WinUI and C++/WinRT can still
				// obtain the original message after this inspection.
				winrt::com_ptr<IErrorInfo> errorInfo;
				if (FAILED(::GetErrorInfo(0, errorInfo.put())) || !errorInfo)
				{
					return {};
				}
				::SetErrorInfo(0, errorInfo.get());

				winrt::com_ptr<ILanguageExceptionStackBackTrace> provider;
				(void)errorInfo->QueryInterface(
					__uuidof(ILanguageExceptionStackBackTrace),
					provider.put_void());

				if (!provider)
				{
					winrt::com_ptr<ILanguageExceptionErrorInfo> languageInfo;
					if (SUCCEEDED(errorInfo->QueryInterface(
						__uuidof(ILanguageExceptionErrorInfo),
						languageInfo.put_void())) && languageInfo)
					{
						winrt::com_ptr<IUnknown> languageException;
						if (SUCCEEDED(languageInfo->GetLanguageException(
							languageException.put())) && languageException)
						{
							(void)languageException->QueryInterface(
								__uuidof(
									ILanguageExceptionStackBackTrace),
								provider.put_void());
						}
					}
				}

				if (!provider)
				{
					return {};
				}

				std::array<UINT_PTR, 64> rawFrames{};
				ULONG frameCount{};
				if (FAILED(provider->GetStackBackTrace(
					static_cast<ULONG>(rawFrames.size()),
					rawFrames.data(),
					&frameCount)))
				{
					return {};
				}

				frameCount = (std::min)(
					frameCount,
					static_cast<ULONG>(rawFrames.size()));
				std::vector<void*> frames;
				frames.reserve(frameCount);
				for (ULONG index = 0; index < frameCount; ++index)
				{
					frames.push_back(
						reinterpret_cast<void*>(rawFrames[index]));
				}
				return frames;
			}
			catch (...)
			{
				return {};
			}
		}

		static std::vector<void*> CaptureHandlerStack()
		{
			std::array<void*, 64> rawFrames{};
			USHORT const frameCount = ::RtlCaptureStackBackTrace(
				2,
				static_cast<ULONG>(rawFrames.size()),
				rawFrames.data(),
				nullptr);
			return std::vector<void*>(
				rawFrames.begin(),
				rawFrames.begin() + frameCount);
		}

		static std::wstring WidenSymbol(char const* value)
		{
			if (!value || !*value)
			{
				return {};
			}

			int length = ::MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				value,
				-1,
				nullptr,
				0);
			UINT codePage = CP_UTF8;
			DWORD flags = MB_ERR_INVALID_CHARS;
			if (length == 0)
			{
				codePage = CP_ACP;
				flags = 0;
				length = ::MultiByteToWideChar(
					codePage,
					flags,
					value,
					-1,
					nullptr,
					0);
			}
			if (length <= 1)
			{
				return {};
			}

			std::wstring result(
				static_cast<size_t>(length),
				L'\0');
			::MultiByteToWideChar(
				codePage,
				flags,
				value,
				-1,
				result.data(),
				length);
			result.pop_back();
			return result;
		}

		static std::wstring FormatStackTrace(
			std::vector<void*> const& frames,
			bool isOriginal)
		{
			std::wostringstream output;
			output << (
				isOriginal
				? L"Original exception stack:\r\n"
				: L"UnhandledException handler stack "
				L"(the exception did not expose its original stack):"
				L"\r\n");

			if (frames.empty())
			{
				output << L"  <no stack frames available>";
				return output.str();
			}

			static std::mutex symbolMutex;
			std::scoped_lock const lock{ symbolMutex };

			HANDLE const process = ::GetCurrentProcess();
			::SymSetOptions(
				::SymGetOptions()
				| SYMOPT_DEFERRED_LOADS
				| SYMOPT_LOAD_LINES
				| SYMOPT_UNDNAME);
			// SymInitialize returns FALSE with ERROR_INVALID_PARAMETER when
			// another component already initialized DbgHelp for this process.
			// Symbol lookup remains valid in that case.
			(void)::SymInitialize(process, nullptr, TRUE);

			for (size_t index = 0; index < frames.size(); ++index)
			{
				DWORD64 const address =
					reinterpret_cast<DWORD64>(frames[index]);
				HMODULE module{};
				wchar_t modulePath[MAX_PATH]{};
				std::wstring moduleName{ L"<unknown>" };
				if (::GetModuleHandleExW(
					GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
					| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCWSTR>(frames[index]),
					&module)
					&& ::GetModuleFileNameW(
						module,
						modulePath,
						static_cast<DWORD>(std::size(modulePath))))
				{
					std::wstring_view path{ modulePath };
					auto const separator = path.find_last_of(L"\\/");
					moduleName.assign(
						separator == std::wstring_view::npos
						? path
						: path.substr(separator + 1));
				}

				alignas(SYMBOL_INFO) std::array<
					std::byte,
					sizeof(SYMBOL_INFO) + MAX_SYM_NAME> symbolStorage{};
				auto* symbol = reinterpret_cast<SYMBOL_INFO*>(
					symbolStorage.data());
				symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
				symbol->MaxNameLen = MAX_SYM_NAME;
				DWORD64 symbolDisplacement{};

				output << std::format(
					L"  #{:02} {}",
					index,
					moduleName);
				if (::SymFromAddr(
					process,
					address,
					&symbolDisplacement,
					symbol))
				{
					output << L"!"
						<< WidenSymbol(symbol->Name)
						<< std::format(
							L"+0x{:X}",
							symbolDisplacement);
				}
				else
				{
					DWORD64 const moduleBase =
						::SymGetModuleBase64(process, address);
					output << std::format(
						L"+0x{:X}",
						moduleBase ? address - moduleBase : address);
				}

				IMAGEHLP_LINEW64 line{};
				line.SizeOfStruct = sizeof(line);
				DWORD lineDisplacement{};
				if (::SymGetLineFromAddrW64(
					process,
					address,
					&lineDisplacement,
					&line)
					&& line.FileName)
				{
					output << std::format(
						L" ({}:{})",
						line.FileName,
						line.LineNumber);
				}
				output << std::format(L" [0x{:X}]\r\n", address);
			}

			return output.str();
		}

		static ExceptionStackTrace CaptureExceptionStackTrace()
		{
			auto frames = TryCaptureLanguageExceptionStack();
			bool const isOriginal = !frames.empty();
			if (!isOriginal)
			{
				frames = CaptureHandlerStack();
			}

			return {
				frames,
				FormatStackTrace(frames, isOriginal)
			};
		}

		static sentry_uuid_t CaptureException(
			HRESULT errorCode,
			winrt::hstring const& message,
			std::vector<void*> const& stackFrames,
			winrt::hstring const& details) noexcept
		{
			try
			{
				auto const utf8Message = winrt::to_string(message);
				auto const utf8Details = winrt::to_string(details);
				auto const utf8ErrorCode = std::format(
					"0x{:08X}",
					static_cast<uint32_t>(errorCode));

				sentry_value_t event = sentry_value_new_event();

				sentry_value_t exception = sentry_value_new_exception(
					"winrt::hresult_error",
					utf8Message.c_str());
				sentry_value_set_stacktrace(
					exception,
					stackFrames.empty()
					? nullptr
					: const_cast<void**>(stackFrames.data()),
					stackFrames.size());

				sentry_value_t mechanism = sentry_value_new_object();
				sentry_value_set_by_key(
					mechanism,
					"type",
					sentry_value_new_string(
						"Microsoft.UI.Xaml.UnhandledException"));
				sentry_value_set_by_key(
					mechanism,
					"handled",
					sentry_value_new_bool(0));

				sentry_value_t mechanismData = sentry_value_new_object();
				sentry_value_set_by_key(
					mechanismData,
					"HRESULT",
					sentry_value_new_string(utf8ErrorCode.c_str()));
				sentry_value_set_by_key(
					mechanism,
					"data",
					mechanismData);
				sentry_value_set_by_key(
					exception,
					"mechanism",
					mechanism);
				sentry_event_add_exception(event, exception);

				// 0.16 scope capture keeps this event's fatal level and WinUI
				// context local instead of mutating the process-wide scope.
				if (sentry_scope_t* scope = sentry_local_scope_new())
				{
					sentry_scope_set_level(scope, SENTRY_LEVEL_FATAL);
					sentry_scope_set_tag(
						scope,
						"exception.source",
						"Microsoft.UI.Xaml");

					sentry_value_t winuiContext = sentry_value_new_object();
					sentry_value_set_by_key(
						winuiContext,
						"hresult",
						sentry_value_new_string(utf8ErrorCode.c_str()));
					sentry_value_set_by_key(
						winuiContext,
						"thread_id",
						sentry_value_new_uint64(GetCurrentThreadId()));
					sentry_scope_set_context(
						scope,
						"winui",
						winuiContext);
					sentry_scope_set_extra(
						scope,
						"exception_details",
						sentry_value_new_string(
							utf8Details.c_str()));
					sentry_scope_attach_bytes(
						scope,
						utf8Details.data(),
						utf8Details.size(),
						"exception-details.txt");

					return sentry_scope_capture_event(scope, event);
				}

				// Preserve capture under severe allocation pressure.
				sentry_value_set_by_key(
					event,
					"level",
					sentry_value_new_string("fatal"));
				return sentry_capture_event(event);
			}
			catch (...)
			{
				// Keep the error UI available even if UTF conversion or event
				// construction fails under memory pressure.
				return sentry_uuid_nil();
			}
		}

		static inline std::atomic_bool s_isHandlingException{ false };
	};
}
