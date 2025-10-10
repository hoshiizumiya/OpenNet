#pragma once
#include <string>
#include <stdexcept>
#include <sstream>
#include <format>
#include <Windows.h>
#include <DbgHelp.h>
#include <vector>
#include <fstream>

#pragma comment(lib, "Dbghelp.lib")

namespace mvvm::exceptions
{
    struct mvvm_exception : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct invalid_object : public mvvm_exception
    {
        using mvvm_exception::mvvm_exception;
    };

    struct invalid_internal_state : public mvvm_exception
    {
        using mvvm_exception::mvvm_exception;
    };

    struct invalid_parameter : public mvvm_exception
    {
        using mvvm_exception::mvvm_exception;
    };
}

namespace mvvm::diagnostics
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error,
        Fatal
    };

    inline const wchar_t* ToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Info: return L"INFO";
        case LogLevel::Warning: return L"WARNING";
        case LogLevel::Error: return L"ERROR";
        case LogLevel::Fatal: return L"FATAL";
        default: return L"UNKNOWN";
        }
    }

    inline std::wstring widen(const char* narrow)
    {
        if (!narrow) return L"(null)";
        int len = MultiByteToWideChar(CP_UTF8, 0, narrow, -1, nullptr, 0);
        std::wstring result(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, narrow, -1, result.data(), len);
        result.pop_back(); // remove null terminator
        return result;
    }

    inline std::string narrow(std::wstring_view wide)
    {
        if (wide.empty()) return {};

        int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), 
            static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string result(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), 
            static_cast<int>(wide.size()), result.data(), len, nullptr, nullptr);
        return result;
    }

    inline std::wstring CurrentTimestamp()
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[64];
        swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        return buf;
    }

#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

    inline std::wstring CaptureStackTrace()
    {
        constexpr int MAX_FRAMES = 30;
        constexpr std::wstring_view systemModules[] = { L"kernel32.dll", L"kernelbase.dll", L"ntdll.dll" };

        void* stack[MAX_FRAMES];
        USHORT frames = RtlCaptureStackBackTrace(0, MAX_FRAMES, stack, nullptr);

        HANDLE process = GetCurrentProcess();
        SymInitialize(process, nullptr, TRUE);

        std::wstringstream ss;
        ss << L"Stack Trace:\n";

        bool inSystemBlock = false;
        std::wstring collapsedModule;
        DWORD collapsedCount = 0;
        DWORD64 firstSystemAddr = 0;
        MODULEINFO modInfo = {};

        for (USHORT i = 2; i < frames; ++i)
        {
            DWORD64 address = (DWORD64)(stack[i]);

            // Get module info
            HMODULE hMod = nullptr;
            WCHAR moduleName[MAX_PATH] = {};

            if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(address), &hMod) &&
                GetModuleInformation(process, hMod, &modInfo, sizeof(modInfo)) &&
                GetModuleFileNameW(hMod, moduleName, MAX_PATH))
            {
                std::wstring modName = moduleName;
                size_t pos = modName.find_last_of(L"\\/");
                if (pos != std::wstring::npos)
                    modName = modName.substr(pos + 1);

                DWORD64 rva = address - (DWORD64)(modInfo.lpBaseOfDll);

                bool isSystem = false;
                for (auto& sys : systemModules)
                {
                    if (_wcsicmp(sys.data(), modName.c_str()) == 0)
                    {
                        isSystem = true;
                        break;
                    }
                }

                if (isSystem)
                {
                    if (!inSystemBlock)
                    {
                        inSystemBlock = true;
                        collapsedModule = modName;
                        firstSystemAddr = address;
                        collapsedCount = 1;
                    }
                    else
                    {
                        collapsedCount++;
                    }
                    continue; // don't output yet
                }
                else
                {
                    if (inSystemBlock)
                    {
                        // Output folded block
                        DWORD64 sysRva = firstSystemAddr - (DWORD64)(modInfo.lpBaseOfDll);
                        ss << L"  |->> [Folded] " << collapsedCount
                            << L" system frame(s) from " << collapsedModule
                            << L" + 0x" << std::hex << sysRva << std::dec << L"\n";
                        inSystemBlock = false;
                        collapsedCount = 0;
                    }

                    // Try get symbol name
                    BYTE symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]{};
                    PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
                    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                    pSymbol->MaxNameLen = MAX_SYM_NAME;

                    DWORD64 displacement = 0;
                    if (SymFromAddr(process, address, &displacement, pSymbol))
                    {
                        // get source file path and line number information
                        IMAGEHLP_LINEW64 line{};
                        line.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
                        DWORD lineDisplacement = 0;

                        ss << L"[" << i << L"] " << modName
                            << L" + 0x" << std::hex << rva
                            << L" (" << pSymbol->Name << L")" << std::dec;

                        #ifdef MVVM_TRACE_WITH_SOURCE
                        bool hasLine = SymGetLineFromAddrW64(process, address, &lineDisplacement, &line);
                        if (hasLine)
                        {
                            ss << L" [" << line.FileName << L":" << line.LineNumber << L"]";
                        }
                        #endif

                        ss << L"\n";

                    }
                }
            }
        }

        // Flush final folded block if still active
        if (inSystemBlock)
        {
            ss << L"  |->> [Folded] " << collapsedCount
                << L" system frame(s) from " << collapsedModule
                << L" + 0x" << std::hex << (firstSystemAddr - (DWORD64)modInfo.lpBaseOfDll) << std::dec << L"\n";
        }

        SymCleanup(process);
        return ss.str();
    }

    inline void OutputLog(std::wstring const& text)
    {
#ifdef _DEBUG
        OutputDebugStringW(text.c_str());
#else
        std::wofstream file(L"mvvm_log.txt", std::ios::app);
        file << text;
#endif
    }

    inline void LogMessage(
        LogLevel level,
        std::wstring_view message,
        std::wstring_view file,
        std::wstring_view function,
        int line,
        bool includeStack = false)
    {
        std::wstringstream ss;
        ss << L"[" << ToString(level) << L"] "
            << CurrentTimestamp() << L" | "
            << file << L":" << line << L" (" << function << L")\n"
            << L"|-> " << message << L"\n";

        if (includeStack)
        {
            ss << CaptureStackTrace();
        }

        ss << L"\n";

        OutputLog(ss.str());

        if (level == LogLevel::Fatal && IsDebuggerPresent())
        {
            __debugbreak();
        }
    }

    // Convenience Macros

    template <size_t N>
    constexpr const char* FilenameFromPath(const char(&path)[N])
    {
        static_assert(N > 1, "FilenameFromPath requires non-empty path");
        const char* last = path;
        for (size_t i = 0; i < N; ++i)
        {
            if (path[i] == '/' || path[i] == '\\')
                last = &path[i + 1];
            if (path[i] == '\0') break;
        }
        return last;
    }

#define __MVVM_FILENAME__ ::mvvm::diagnostics::FilenameFromPath(__FILE__)

#ifdef MVVM_LOG_VERBOSE

#define MVVM_LOG(level, msg) \
    do { \
        ::mvvm::diagnostics::LogMessage(level, msg, \
            ::mvvm::diagnostics::widen(__MVVM_FILENAME__), \
            ::mvvm::diagnostics::widen(__FUNCTION__), __LINE__); \
    } while(false)

#define MVVM_LOG_STACK(level, msg) \
    do { \
        ::mvvm::diagnostics::LogMessage(level, msg, \
            ::mvvm::diagnostics::widen(__MVVM_FILENAME__), \
            ::mvvm::diagnostics::widen(__FUNCTION__), __LINE__, true); \
    } while(false)

#else

#define MVVM_LOG(level, msg) \
    do { \
        if constexpr (level != ::mvvm::diagnostics::LogLevel::Info) { \
            ::mvvm::diagnostics::LogMessage(level, msg, \
                ::mvvm::diagnostics::widen(__MVVM_FILENAME__), \
                ::mvvm::diagnostics::widen(__FUNCTION__), __LINE__); \
        } \
    } while(false)

#define MVVM_LOG_STACK(level, msg) \
    do { \
        if constexpr (level != ::mvvm::diagnostics::LogLevel::Info) { \
            ::mvvm::diagnostics::LogMessage(level, msg, \
                ::mvvm::diagnostics::widen(__MVVM_FILENAME__), \
                ::mvvm::diagnostics::widen(__FUNCTION__), __LINE__, true); \
        } \
    } while(false)

#endif

#define MVVM_WARN(msg)    MVVM_LOG(::mvvm::diagnostics::LogLevel::Warning, msg)
#define MVVM_ERROR(msg)   MVVM_LOG(::mvvm::diagnostics::LogLevel::Error, msg)
#define MVVM_FATAL(msg)   MVVM_LOG(::mvvm::diagnostics::LogLevel::Fatal, msg)

#ifndef MVVM_LOG_VERBOSE
#define MVVM_INFO(msg)    ((void)0)
#else
#define MVVM_INFO(msg)    MVVM_LOG(::mvvm::diagnostics::LogLevel::Info, msg)
#endif
    
#define MVVM_THROW(ex_type, msg) \
    do { \
        MVVM_LOG_STACK(::mvvm::diagnostics::LogLevel::Fatal, msg); \
        throw ::mvvm::exceptions::ex_type{ ::mvvm::diagnostics::narrow(msg) };  \
    } while(false)

} // namespace mvvm::diagnostics
