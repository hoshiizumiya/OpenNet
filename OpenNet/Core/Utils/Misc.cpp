#include "pch.h"
#include "Misc.h"
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.System.UserProfile.h>

/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include <optional>
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <cctype>
#include <windows.h>

#include <boost/version.hpp>
#include <libtorrent/version.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <zlib.h>

namespace
{
    // Unit display strings
    constexpr std::wstring_view units[] =
    {
        L"B",       // bytes
        L"KiB",     // kibibytes (1024 bytes)
        L"MiB",     // mebibytes (1024 kibibytes)
        L"GiB",     // gibibytes (1024 mibibytes)
        L"TiB",     // tebibytes (1024 gibibytes)
        L"PiB",     // pebibytes (1024 tebibytes)
        L"EiB"      // exbibytes (1024 pebibytes)
    };

    constexpr const wchar_t* INFINITY_STR = L"∞";
    constexpr const wchar_t* TORRENT_FILE_EXTENSION = L".torrent";

    // return best user friendly storage unit (B, KiB, MiB, GiB, TiB, ...)
    struct SplitToFriendlyUnitResult
    {
        double value;
        Utils::Misc::SizeUnit unit;
    };

    std::optional<SplitToFriendlyUnitResult> splitToFriendlyUnit(const int64_t bytes, const int unitThreshold = 1024)
    {
        if (bytes < 0)
            return std::nullopt;

        int i = 0;
        double value = static_cast<double>(bytes);

        while ((value >= unitThreshold) && (i < static_cast<int>(Utils::Misc::SizeUnit::ExbiByte)))
        {
            value /= 1024.0;
            ++i;
        }
        return { {value, static_cast<Utils::Misc::SizeUnit>(i)} };
    }

    std::wstring doubleToWString(double value, int precision)
    {
        std::wostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        std::wstring result = oss.str();

        // Remove trailing zeros after decimal point
        if (precision > 0 && result.find(L'.') != std::wstring::npos)
        {
            result.erase(result.find_last_not_of(L'0') + 1, std::wstring::npos);
            if (result.back() == L'.')
                result.pop_back();
        }

        return result;
    }

    // Convert locale string to lowercase for comparison
    std::wstring toLower(std::wstring_view str)
    {
        std::wstring result(str);
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    // Check if string starts with prefix (case-insensitive)
    bool startsWithIgnoreCase(std::wstring_view str, std::wstring_view prefix)
    {
        if (str.length() < prefix.length())
            return false;
        return toLower(str.substr(0, prefix.length())) == toLower(prefix);
    }

    // Check if string ends with suffix (case-insensitive)
    bool endsWithIgnoreCase(std::wstring_view str, std::wstring_view suffix)
    {
        if (str.length() < suffix.length())
            return false;
        size_t pos = str.length() - suffix.length();
        return toLower(str.substr(pos)) == toLower(suffix);
    }
}


winrt::hstring Utils::Misc::unitString(const SizeUnit unit, const bool isSpeed)
{
    const auto& unitStr = units[static_cast<int>(unit)];
    winrt::hstring result{ unitStr };
    if (isSpeed)
        result = result + L"/s";
    return result;
}

winrt::hstring Utils::Misc::friendlyUnit(const int64_t bytes, const bool isSpeed, const int precision)
{
    const std::optional<SplitToFriendlyUnitResult> result = splitToFriendlyUnit(bytes);
    if (!result)
        return L"Unknown";

    const int digitPrecision = (precision >= 0) ? precision : friendlyUnitPrecision(result->unit);
    std::wstring valueStr = doubleToWString(result->value, digitPrecision);

    const auto index = static_cast<size_t>(result->unit);

    std::wstring finalStr;
    finalStr.reserve(valueStr.size() + 1 + units[index].size());

    finalStr.append(valueStr);
    finalStr.append(L" ");
    finalStr.append(units[index]);

    return winrt::hstring{ finalStr };
}

winrt::hstring Utils::Misc::friendlyUnitCompact(const int64_t bytes)
{
    // avoid 1000-1023 values, use next larger unit instead
    const std::optional<SplitToFriendlyUnitResult> result = splitToFriendlyUnit(bytes, 1000);
    if (!result)
        return L"Unknown";

    int precision = 0;          // >= 100
    if (result->value < 10)
        precision = 2;          // 0 - 9.99
    else if (result->value < 100)
        precision = 1;          // 10 - 99.9

    std::wstring valueStr = doubleToWString(result->value, precision);
    // use only one character for unit representation
    std::wstring unit(units[static_cast<int>(result->unit)]);

    return winrt::hstring{ valueStr + L" " + unit.substr(0, 1) };
}

int Utils::Misc::friendlyUnitPrecision(const SizeUnit unit)
{
    // friendlyUnit's number of digits after the decimal point
    switch (unit)
    {
        case SizeUnit::Byte:
            return 0;
        case SizeUnit::KibiByte:
        case SizeUnit::MebiByte:
            return 1;
        case SizeUnit::GibiByte:
            return 2;
        default:
            return 3;
    }
}

int64_t Utils::Misc::sizeInBytes(double size, const Utils::Misc::SizeUnit unit)
{
    for (int i = 0; i < static_cast<int>(unit); ++i)
        size *= 1024.0;
    return static_cast<int64_t>(size);
}

bool Utils::Misc::isPreviewable(const std::wstring& filePath)
{
    // Get file extension
    size_t lastDot = filePath.find_last_of(L'.');
    if (lastDot == std::wstring::npos)
        return false;

    std::wstring extension = filePath.substr(lastDot);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::toupper);

    static const std::unordered_set<std::wstring> multimediaExtensions =
    {
        L".3GP", L".AAC", L".AC3", L".AIF", L".AIFC", L".AIFF", L".ASF", L".AU", L".AVI", L".FLAC",
        L".FLV", L".M3U", L".M4A", L".M4P", L".M4V", L".MID", L".MKV", L".MOV", L".MP2", L".MP3",
        L".MP4", L".MPC", L".MPE", L".MPEG", L".MPG", L".MPP", L".OGG", L".OGM", L".OGV", L".QT",
        L".RA", L".RAM", L".RM", L".RMV", L".RMVB", L".SWA", L".SWF", L".TS", L".VOB", L".WAV",
        L".WMA", L".WMV"
    };

    return multimediaExtensions.find(extension) != multimediaExtensions.end();
}

bool Utils::Misc::isTorrentLink(const winrt::hstring& str)
{
    std::wstring link{ str };
    return startsWithIgnoreCase(link, L"magnet:")
        || endsWithIgnoreCase(link, TORRENT_FILE_EXTENSION);
}

winrt::hstring Utils::Misc::userFriendlyDuration(const int64_t seconds, const int64_t maxCap, const TimeResolution resolution)
{
    if (seconds < 0)
        return INFINITY_STR;
    if ((maxCap >= 0) && (seconds >= maxCap))
        return INFINITY_STR;

    if (seconds == 0)
        return L"0";

    if (seconds < 60)
    {
        if (resolution == TimeResolution::Minutes)
            return L"< 1m";

        std::wostringstream oss;
        oss << seconds << L"s";
        return winrt::hstring{ oss.str() };
    }

    int64_t minutes = (seconds / 60);
    if (minutes < 60)
    {
        std::wostringstream oss;
        oss << minutes << L"m";
        return winrt::hstring{ oss.str() };
    }

    int64_t hours = (minutes / 60);
    if (hours < 24)
    {
        minutes -= (hours * 60);
        std::wostringstream oss;
        oss << hours << L"h " << minutes << L"m";
        return winrt::hstring{ oss.str() };
    }

    int64_t days = (hours / 24);
    if (days < 365)
    {
        hours -= (days * 24);
        std::wostringstream oss;
        oss << days << L"d " << hours << L"h";
        return winrt::hstring{ oss.str() };
    }

    int64_t years = (days / 365);
    days -= (years * 365);
    std::wostringstream oss;
    oss << years << L"y " << days << L"d";
    return winrt::hstring{ oss.str() };
}

winrt::hstring Utils::Misc::languageToLocalizedString(const std::wstring_view localeStr)
{
    std::wstring locale = toLower(localeStr);

    if (locale.find(L"eo") == 0)
        return L"Esperanto";

    if (locale.find(L"ltg") == 0)
        return L"Latgalian";

    // Extract primary language tag and territory if present
    std::wstring langTag = locale;
    size_t dashPos = locale.find(L'-');
    if (dashPos != std::wstring::npos)
        langTag = locale.substr(0, dashPos);

    // Simplified mapping - returns language name or the input if not recognized
    if (langTag == L"ar") return L"العربية";
    if (langTag == L"hy") return L"Հայերեն";
    if (langTag == L"az") return L"Azərbaycanca";
    if (langTag == L"eu") return L"Euskera";
    if (langTag == L"bg") return L"Български";
    if (langTag == L"be") return L"Беларуская";
    if (langTag == L"ca") return L"Català";
    if (langTag == L"zh")
    {
        if (locale.find(L"hk") != std::wstring::npos || locale.find(L"mo") != std::wstring::npos)
            return L"繁體中文 (香港)";
        if (locale.find(L"tw") != std::wstring::npos)
            return L"繁體中文 (臺灣)";
        return L"简体中文";
    }
    if (langTag == L"hr") return L"Hrvatski";
    if (langTag == L"cs") return L"Čeština";
    if (langTag == L"da") return L"Dansk";
    if (langTag == L"nl") return L"Nederlands";
    if (langTag == L"en")
    {
        if (locale.find(L"au") != std::wstring::npos)
            return L"English (Australia)";
        if (locale.find(L"gb") != std::wstring::npos)
            return L"English (United Kingdom)";
        return L"English";
    }
    if (langTag == L"et") return L"Eesti";
    if (langTag == L"fi") return L"Suomi";
    if (langTag == L"fr") return L"Français";
    if (langTag == L"gl") return L"Galego";
    if (langTag == L"ka") return L"ქართული";
    if (langTag == L"de") return L"Deutsch";
    if (langTag == L"el") return L"Ελληνικά";
    if (langTag == L"he") return L"עברית";
    if (langTag == L"hi") return L"हिन्दी";
    if (langTag == L"hu") return L"Magyar";
    if (langTag == L"is") return L"Íslenska";
    if (langTag == L"id") return L"Bahasa Indonesia";
    if (langTag == L"it") return L"Italiano";
    if (langTag == L"ja") return L"日本語";
    if (langTag == L"ko") return L"한국어";
    if (langTag == L"lv") return L"Latviešu";
    if (langTag == L"lt") return L"Lietuvių";
    if (langTag == L"ms") return L"Bahasa Melayu";
    if (langTag == L"mn") return L"Монгол";
    if (langTag == L"nb") return L"Norsk";
    if (langTag == L"oc") return L"Occitan";
    if (langTag == L"fa") return L"فارسی";
    if (langTag == L"pl") return L"Polski";
    if (langTag == L"pt")
    {
        if (locale.find(L"br") != std::wstring::npos)
            return L"Português (Brasil)";
        return L"Português";
    }
    if (langTag == L"ro") return L"Română";
    if (langTag == L"ru") return L"Русский";
    if (langTag == L"sr") return L"Српски";
    if (langTag == L"sk") return L"Slovenčina";
    if (langTag == L"sl") return L"Slovenščina";
    if (langTag == L"es") return L"Español";
    if (langTag == L"sv") return L"Svenska";
    if (langTag == L"th") return L"ไทย";
    if (langTag == L"tr") return L"Türkçe";
    if (langTag == L"uk") return L"Українська";
    if (langTag == L"uz") return L"Ўзбек";
    if (langTag == L"vi") return L"Tiếng Việt";

    return winrt::hstring{ localeStr };
}

winrt::hstring Utils::Misc::parseHtmlLinks(const winrt::hstring& rawText)
{
    std::wstring result{ rawText };

    // Simple regex for detecting URLs
    // Pattern: starts with http(s)://domain or just domain.extension
    static const std::wregex reURL(
        LR"((\s|^)(https?://([a-zA-Z0-9._-]+\.)+[a-zA-Z0-9/?%=&#;:-]+|([a-zA-Z0-9._-]+\.)+[a-zA-Z]{2,}))",
        std::wregex::ECMAScript | std::wregex::icase
    );

    // Replace URLs with links
    std::wstring replaced;
    std::wsregex_iterator it(result.begin(), result.end(), reURL);
    std::wsregex_iterator end;
    size_t lastPos = 0;

    for (; it != end; ++it)
    {
        replaced += result.substr(lastPos, it->position(0) - lastPos);

        std::wstring url = it->str();
        std::wstring prefix;

        // Check for whitespace prefix
        if (iswspace(url[0]))
        {
            prefix = url.substr(0, 1);
            url = url.substr(1);
        }

        // Add scheme if missing
        if (url.find(L"http://") == std::wstring::npos && 
            url.find(L"https://") == std::wstring::npos)
        {
            url = L"http://" + url;
        }

        replaced += prefix + L"<a href=\"" + url + L"\">" + url + L"</a>";
        lastPos = it->position(0) + it->length(0);
    }

    replaced += result.substr(lastPos);

    // Wrap in paragraph tag
    result = L"<p style=\"white-space: pre-wrap;\">" + replaced + L"</p>";
    return winrt::hstring{ result };
}

winrt::hstring Utils::Misc::osName()
{
    // Get Windows version info
    DWORD size = GetEnvironmentVariableW(L"OS", nullptr, 0);
    std::wstring osName;
    if (size > 0)
    {
        osName.resize(size - 1);
        GetEnvironmentVariableW(L"OS", osName.data(), size);
    }
    else
    {
        osName = L"Windows";
    }

    // Get processor info
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    std::wstring arch;
    switch (sysInfo.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = L"x64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: arch = L"x86"; break;
        case PROCESSOR_ARCHITECTURE_ARM: arch = L"ARM"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = L"ARM64"; break;
        default: arch = L"Unknown";
    }

    std::wostringstream oss;
    oss << osName << L" " << arch;
    return winrt::hstring{ oss.str() };
}

winrt::hstring Utils::Misc::boostVersionString()
{
    std::wostringstream oss;
    oss << BOOST_VERSION / 100000 << L"."
        << (BOOST_VERSION / 100) % 1000 << L"."
        << BOOST_VERSION % 100;
    return winrt::hstring{ oss.str() };
}

winrt::hstring Utils::Misc::libtorrentVersionString()
{
    const char* versionStr = lt::version();
    int len = std::strlen(versionStr);
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, versionStr, len, nullptr, 0);

    std::wstring wideVersion(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, versionStr, len, wideVersion.data(), wideLen);

    return winrt::hstring{ wideVersion };
}

winrt::hstring Utils::Misc::opensslVersionString()
{
    const char* versionStr = ::OpenSSL_version(OPENSSL_VERSION);
    int len = std::strlen(versionStr);
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, versionStr, len, nullptr, 0);

    std::wstring wideVersion(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, versionStr, len, wideVersion.data(), wideLen);

    // Extract version number (second word)
    size_t spacePos = wideVersion.find(L' ');
    if (spacePos != std::wstring::npos)
    {
        size_t nextSpace = wideVersion.find(L' ', spacePos + 1);
        if (nextSpace != std::wstring::npos)
            return winrt::hstring{ wideVersion.substr(spacePos + 1, nextSpace - spacePos - 1) };
    }

    return winrt::hstring{ wideVersion };
}

winrt::hstring Utils::Misc::zlibVersionString()
{
    const char* versionStr = zlibVersion();
    int len = std::strlen(versionStr);
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, versionStr, len, nullptr, 0);

    std::wstring wideVersion(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, versionStr, len, wideVersion.data(), wideLen);

    return winrt::hstring{ wideVersion };
}