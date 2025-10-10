//*********************************************************
//
//    Copyright (c) Millennium R&D Team. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
#pragma once
#ifndef __MVVM_CPPWINRT_NAME_OF_H_INCLUDED
#define __MVVM_CPPWINRT_NAME_OF_H_INCLUDED

#include <string_view>
using namespace std::literals;

/// <summary>
/// Gets a <c>std::wstring_view</c> containing the name of the specified <paramref cref="propertyName"/> token,
/// and statically verifies the spelling of the property name at compile time.
/// </summary>
/// <description>
/// NAME_OF (and NAME_OF_NARROW) can be used to get the name of a property as a wstring_view
/// or string_view constexpr. This is commonly needed when checking which property changed after receiving a
/// PropertyChanged event, and also when raising that event from a view model. It will enforce at compile
/// time that propertyName is a member of typeName, then return propertyName as a wstring_view or string_view
/// constexpr. There is no additional runtime overhead to using this as opposed to hard coding string
/// literals directly or in global variables when optimized, because the Microsoft compiler de-duplicates
/// string literals when O1 or O2 are enabled. The macros work by using a ternary expression which the compiler
/// can understand at compilation will always be false, leaving only the string_view to propertyName. By
/// returing a wstring_view or string_view, the results of these can be directly compared to a wchar_t* or
/// char*, respectively (const or not).
/// _NAME_OF_POINTER_TYPET is necessary, because this macro will likely be used within conditional statements
/// where defining a type is not syntactically correct.
/// _NAME_OF_MAKE_FALSE is necessary, because we should not assume the property type can be converted to
/// a bool by the compiler. Although this code never runs, it must be syntactically correct for all cases.
/// </description>
#define NAME_OF(typeName, propertyName) \
__pragma(warning(push)) \
__pragma(warning(disable : 6237)) \
    (false && details::_NAME_OF_MAKE_FALSE(details::_NAME_OF_POINTER_TYPET<typeName>()->propertyName()) ? L""sv : []{ constexpr auto name = (L""#propertyName##sv); return name; }() ) \
__pragma(warning(pop))

/// <summary>
/// Gets a <c>std::string_view</c> containing the name of the specified <paramref cref="propertyName"/> token,
/// and statically verifies the spelling of the property name at compile time.
/// </summary>
#define NAME_OF_NARROW(typeName, propertyName) \
__pragma(warning(push)) \
__pragma(warning(disable : 6237)) \
    (false && details::_NAME_OF_MAKE_FALSE(details::_NAME_OF_POINTER_TYPET<typeName>()->propertyName()) ? ""sv : []{ constexpr auto name = (""#propertyName##sv); return name; }() ) \
__pragma(warning(pop))

namespace details
{
    template<typename T>
    constexpr T* _NAME_OF_POINTER_TYPET()
    {
        T* returnValue;
        return returnValue;
    }

    template<typename T>
    constexpr bool _NAME_OF_MAKE_FALSE(T)
    {
        return false;
    }
}

// This can be used to test the optimization at godbolt.org
#if 0
struct ViewModel
{
    int MyProperty() { return 0; }
};

void wfoo(std::wstring_view const&);
void afoo(std::string_view const&);

void bar()
{
    wfoo(NAME_OF(ViewModel, MyProperty));
    afoo(NAME_OF_NARROW(ViewModel, MyProperty));
}
#endif

#endif // __MVVM_CPPWINRT_NAME_OF_H_INCLUDED
