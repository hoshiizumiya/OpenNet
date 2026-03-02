/*
 * PROJECT:   OpenNet
 * FILE:      Core/Aria2/JsonRpc2.cpp
 * PURPOSE:   JSON-RPC 2.0 protocol implementation
 *            (migrated from NanaGet.JsonRpc2.cpp)
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/Aria2/JsonRpc2.h"
#include "Core/Aria2/Aria2Helpers.h"

using namespace OpenNet::Core::Aria2::Helpers;

std::string OpenNet::Core::Aria2::JsonRpc2::FromRequestMessage(
    RequestMessage const &Value)
{
    try
    {
        nlohmann::json j;
        j["jsonrpc"] = "2.0";
        j["method"] = Value.Method;
        j["params"] = nlohmann::json::parse(Value.Parameters);
        j["id"] = Value.Identifier;
        return j.dump(2);
    }
    catch (...)
    {
        return {};
    }
}

bool OpenNet::Core::Aria2::JsonRpc2::ToNotificationMessage(
    std::string const &Source,
    NotificationMessage &Destination)
{
    nlohmann::json src;
    try
    {
        src = nlohmann::json::parse(Source);
    }
    catch (...)
    {
        return false;
    }

    Destination.Method = JsonToString(JsonGetSubKey(src, "method"));

    auto params = JsonGetSubKey(src, "params");
    if (!params.is_null())
    {
        Destination.Parameters = params.dump(2);
    }
    return true;
}

std::string OpenNet::Core::Aria2::JsonRpc2::FromErrorMessage(
    ErrorMessage const &Value)
{
    try
    {
        nlohmann::json j;
        j["code"] = Value.Code;
        j["message"] = Value.Message;
        j["data"] = nlohmann::json::parse(Value.Data);
        return j.dump(2);
    }
    catch (...)
    {
        return {};
    }
}

OpenNet::Core::Aria2::JsonRpc2::ErrorMessage
OpenNet::Core::Aria2::JsonRpc2::ToErrorMessage(nlohmann::json const &Value)
{
    ErrorMessage result;
    result.Code = JsonToInt64(JsonGetSubKey(Value, "code"));
    result.Message = JsonToString(JsonGetSubKey(Value, "message"));
    auto data = JsonGetSubKey(Value, "data");
    if (!data.is_null())
        result.Data = data.dump(2);
    return result;
}

bool OpenNet::Core::Aria2::JsonRpc2::ToResponseMessage(
    std::string const &Source,
    ResponseMessage &Destination)
{
    nlohmann::json src;
    try
    {
        src = nlohmann::json::parse(Source);
    }
    catch (...)
    {
        return false;
    }

    auto jsonrpc = JsonToString(JsonGetSubKey(src, "jsonrpc"));
    if (jsonrpc != "2.0")
        return false;

    auto id = JsonGetSubKey(src, "id");
    if (id.is_null())
        return false;
    Destination.Identifier = JsonToString(id);

    auto msg = JsonGetSubKey(src, "result");
    Destination.IsSucceeded = !msg.is_null();
    if (!Destination.IsSucceeded)
    {
        msg = JsonGetSubKey(src, "error");
        if (msg.is_null())
            return false;
    }
    Destination.Message = msg.dump(2);
    return true;
}
