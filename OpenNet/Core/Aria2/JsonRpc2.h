/*
 * PROJECT:   OpenNet
 * FILE:      Core/Aria2/JsonRpc2.h
 * PURPOSE:   JSON-RPC 2.0 protocol definitions
 *            (migrated from NanaGet.JsonRpc2.h)
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace OpenNet::Core::Aria2::JsonRpc2
{
    struct RequestMessage
    {
        std::string Method;
        std::string Parameters;
        std::string Identifier;
    };

    std::string FromRequestMessage(RequestMessage const &Value);

    struct NotificationMessage
    {
        std::string Method;
        std::string Parameters;
    };

    bool ToNotificationMessage(
        std::string const &Source,
        NotificationMessage &Destination);

    struct ErrorMessage
    {
        std::int64_t Code = 0;
        std::string Message;
        std::string Data;
    };

    std::string FromErrorMessage(ErrorMessage const &Value);
    ErrorMessage ToErrorMessage(nlohmann::json const &Value);

    struct ResponseMessage
    {
        bool IsSucceeded = false;
        std::string Message;
        std::string Identifier;
    };

    bool ToResponseMessage(
        std::string const &Source,
        ResponseMessage &Destination);
}
