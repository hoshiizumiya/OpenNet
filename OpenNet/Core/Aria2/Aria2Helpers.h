/*
 * PROJECT:   OpenNet
 * FILE:      Core/Aria2/Aria2Helpers.h
 * PURPOSE:   Helper utilities for Aria2 integration
 *            (replaces Mile.Json / Mile.Helpers used in NanaGet)
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>
#include <charconv>
#include <memory>

#include <nlohmann/json.hpp>

#include <appmodel.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>

namespace OpenNet::Core::Aria2::Helpers
{
    // ---------------------------------------------------------------
    //  JSON helpers  (replaces Mile::Json::*)
    // ---------------------------------------------------------------

    inline std::string JsonToString(nlohmann::json const& v)
    {
        if (v.is_string()) return v.get<std::string>();
        if (v.is_null())   return {};
        return v.dump();
    }

    inline nlohmann::json JsonGetSubKey(nlohmann::json const& v, std::string const& key)
    {
        if (v.is_object() && v.contains(key)) return v[key];
        return nlohmann::json();
    }

    inline nlohmann::json JsonToArray(nlohmann::json const& v)
    {
        if (v.is_array()) return v;
        return nlohmann::json::array();
    }

    inline nlohmann::json JsonToObject(nlohmann::json const& v)
    {
        if (v.is_object()) return v;
        return nlohmann::json::object();
    }

    inline nlohmann::json JsonToPrimitive(nlohmann::json const& v)
    {
        return v; // pass-through
    }

    inline std::int64_t JsonToInt64(nlohmann::json const& v)
    {
        if (v.is_number()) return v.get<std::int64_t>();
        return 0;
    }

    // ---------------------------------------------------------------
    //  Numeric helpers (replaces Mile::ToUInt64, etc.)
    // ---------------------------------------------------------------

    inline std::uint64_t ToUInt64(std::string const& s, int base = 10)
    {
        if (s.empty()) return 0;
        try { return std::stoull(s, nullptr, base); }
        catch (...) { return 0; }
    }

    inline std::uint32_t ToUInt32(std::string const& s, int base = 10)
    {
        if (s.empty()) return 0;
        try { return static_cast<std::uint32_t>(std::stoul(s, nullptr, base)); }
        catch (...) { return 0; }
    }

    inline std::int32_t ToInt32(std::string const& s, int base = 10)
    {
        if (s.empty()) return 0;
        try { return std::stoi(s, nullptr, base); }
        catch (...) { return 0; }
    }

    // ---------------------------------------------------------------
    //  String formatting (replaces Mile::FormatString / FormatWideString)
    // ---------------------------------------------------------------

    template <typename... Args>
    inline std::string FormatString(const char* fmt, Args... args)
    {
        int sz = std::snprintf(nullptr, 0, fmt, args...);
        if (sz <= 0) return {};
        std::string buf(static_cast<std::size_t>(sz) + 1, '\0');
        std::snprintf(buf.data(), buf.size(), fmt, args...);
        buf.resize(static_cast<std::size_t>(sz));
        return buf;
    }

    template <typename... Args>
    inline std::wstring FormatWideString(const wchar_t* fmt, Args... args)
    {
        // Calculate needed buffer size using _scwprintf (safe count version)
        int sz = _scwprintf(fmt, args...);
        if (sz <= 0) return {};
        std::wstring buf(static_cast<std::size_t>(sz) + 1, L'\0');
        _snwprintf_s(buf.data(), buf.size(), _TRUNCATE, fmt, args...);
        buf.resize(static_cast<std::size_t>(sz));
        return buf;
    }

    // ---------------------------------------------------------------
    //  Scope-exit handler (replaces Mile::ScopeExitTaskHandler)
    // ---------------------------------------------------------------

    template <typename F>
    class ScopeExit
    {
    public:
        explicit ScopeExit(F&& fn) : m_fn(std::move(fn)), m_active(true) {}
        ~ScopeExit() { if (m_active) m_fn(); }
        void Dismiss() { m_active = false; }
        ScopeExit(ScopeExit&& o) noexcept : m_fn(std::move(o.m_fn)), m_active(o.m_active) { o.m_active = false; }
        ScopeExit(ScopeExit const&) = delete;
        ScopeExit& operator=(ScopeExit const&) = delete;
    private:
        F m_fn;
        bool m_active;
    };

    template <typename F>
    inline ScopeExit<F> MakeScopeExit(F&& fn) { return ScopeExit<F>(std::forward<F>(fn)); }

    // ---------------------------------------------------------------
    //  Packaged mode check (replaces Mile::WinRT::IsPackagedMode)
    // ---------------------------------------------------------------

    inline bool IsPackagedMode()
    {
        UINT32 length = 0;
        auto rc = ::GetCurrentPackageFullName(&length, nullptr);
        return rc != APPMODEL_ERROR_NO_PACKAGE;
    }
}
