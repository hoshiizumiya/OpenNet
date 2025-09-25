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
#ifndef __MVVM_CPPWINRT_PROPERTY_MACROS_H_INCLUDED
#define __MVVM_CPPWINRT_PROPERTY_MACROS_H_INCLUDED

#include <string_view>
using namespace std::literals;


#ifndef DEFINE_EVENT
#define DEFINE_EVENT(type, name) \
    private: \
        winrt::event<type> m_event##name; \
    public: \
        winrt::event_token name##(type const& handler) \
        { \
            return m_event##name.add(handler); \
        } \
        void name##(winrt::event_token token) \
        { \
            m_event##name.remove(token); \
        }
#endif


#ifndef DEFINE_PROPERTY_READONLY
#define DEFINE_PROPERTY_READONLY(type, name, defaultValue) \
    public: \
        type name##() { return m_property##name; } \
    private: \
        type m_property##name = defaultValue; \
    public:
#endif


#ifndef _DEFINE_PROPERTY_SCOPED_ACCESS_SET
#define _DEFINE_PROPERTY_SCOPED_ACCESS_SET(type, name, access, defaultValue) \
    public: \
        type name##() { return this->get_property(m_property##name); } \
    access##: \
        void name##(type newValue) \
        { \
            constexpr auto propertyName = L""#name##sv; \
            this->set_property(m_property##name, newValue, propertyName); \
        } \
    private: \
        type m_property##name = defaultValue; \
    public:
#endif

#ifndef DEFINE_PROPERTY
#define DEFINE_PROPERTY(type, name, defaultValue) _DEFINE_PROPERTY_SCOPED_ACCESS_SET(type, name, public, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_PRIVATE_SET
#define DEFINE_PROPERTY_PRIVATE_SET(type, name, defaultValue) _DEFINE_PROPERTY_SCOPED_ACCESS_SET(type, name, private, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_PROTECTED_SET
#define DEFINE_PROPERTY_PROTECTED_SET(type, name, defaultValue) _DEFINE_PROPERTY_SCOPED_ACCESS_SET(type, name, private, defaultValue)
#endif

// [Ly-Fixed]: use newValue instead of value to avoid undefine reference error. 
#ifndef _DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_SCOPED_ACCESS_SET
#define _DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, access, defaultValue) \
    public: \
        type name##() { return this->get_property(m_property##name); } \
    access##: \
        void name##(type newValue) \
        { \
            type oldValue; \
            if (this->set_property(m_property##name, newValue, oldValue)) \
            { \
                this->On##name##Changed(oldValue, newValue); \
            } \
        } \
    protected: \
        void On##name##Changed(type const& oldValue, type const& newValue); \
    private: \
        type m_property##name = defaultValue; \
    public:
#endif

#ifndef DEFINE_PROPERTY_CALLBACK_NO_NOTIFY
#define DEFINE_PROPERTY_CALLBACK_NO_NOTIFY(type, name, defaultValue) _DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, public, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_PRIVATE_SET
#define DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_PRIVATE_SET(type, name, defaultValue) _DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, private, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_PROTECTED_SET
#define DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_PROTECTED_SET(type, name, defaultValue) _DEFINE_PROPERTY_CALLBACK_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, protected, defaultValue)
#endif


#ifndef _DEFINE_PROPERTY_CALLBACK_SCOPED_ACCESS_SET
#define _DEFINE_PROPERTY_CALLBACK_SCOPED_ACCESS_SET(type, name, access, defaultValue) \
    public: \
        type name##() { return this->get_property(m_property##name); } \
    access##: \
        void name##(type newValue) \
        { \
            constexpr auto propertyName = L""#name##sv; \
            type oldValue; \
            if (this->set_property(m_property##name, newValue, oldValue, propertyName)) \
            { \
                this->On##name##Changed(oldValue, newValue); \
            } \
        } \
    protected: \
        void On##name##Changed(type const& oldValue, type const& newValue); \
    private: \
        type m_property##name = defaultValue; \
    public:
#endif

#ifndef DEFINE_PROPERTY_CALLBACK
#define DEFINE_PROPERTY_CALLBACK(type, name, defaultValue) _DEFINE_PROPERTY_CALLBACK_SCOPED_ACCESS_SET(type, name, public, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_CALLBACK_PRIVATE_SET
#define DEFINE_PROPERTY_CALLBACK_PRIVATE_SET(type, name, defaultValue) _DEFINE_PROPERTY_CALLBACK_SCOPED_ACCESS_SET(type, name, private, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_CALLBACK_PROTECTED_SET
#define DEFINE_PROPERTY_CALLBACK_PROTECTED_SET(type, name, defaultValue) _DEFINE_PROPERTY_CALLBACK_SCOPED_ACCESS_SET(type, name, protected, defaultValue)
#endif


#ifndef _DEFINE_PROPERTY_NO_NOTIFY_SCOPED_ACCESS_SET
#define _DEFINE_PROPERTY_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, access, defaultValue) \
public: \
    type name##(){ return this->get_property(m_property##name); } \
access##: \
    void name##(type newValue) { this->set_property(m_property##name, newValue); } \
private: \
    type m_property##name = defaultValue; \
public:
#endif

#ifndef DEFINE_PROPERTY_NO_NOTIFY
#define DEFINE_PROPERTY_NO_NOTIFY(type, name, defaultValue) _DEFINE_PROPERTY_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, public, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_NO_NOTIFY_PRIVATE_SET
#define DEFINE_PROPERTY_NO_NOTIFY_PRIVATE_SET(type, name, defaultValue) _DEFINE_PROPERTY_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, private, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_NO_NOTIFY_PROTECTED_SET
#define DEFINE_PROPERTY_NO_NOTIFY_PROTECTED_SET(type, name, defaultValue) _DEFINE_PROPERTY_NO_NOTIFY_SCOPED_ACCESS_SET(type, name, protected, defaultValue)
#endif


#ifndef _DEFINE_PROPERTY_NO_COMPARE_SCOPED_ACCESS_SET
#define _DEFINE_PROPERTY_NO_COMPARE_SCOPED_ACCESS_SET(type, name, access, defaultValue) \
public: \
    type name##(){ return this->get_property(m_property##name); } \
access##: \
    void name##(type value) \
    { \
        constexpr auto propertyName = L""#name##sv; \
        this->set_property_no_compare(m_property##name, value, propertyName); \
    } \
private: \
    type m_property##name = defaultValue; \
public:
#endif

#ifndef DEFINE_PROPERTY_NO_COMPARE
#define DEFINE_PROPERTY_NO_COMPARE(type, name, defaultValue) _DEFINE_PROPERTY_NO_COMPARE_SCOPED_ACCESS_SET(type, name, public, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_NO_COMPARE_PRIVATE_SET
#define DEFINE_PROPERTY_NO_COMPARE_PRIVATE_SET(type, name, defaultValue) _DEFINE_PROPERTY_NO_COMPARE_SCOPED_ACCESS_SET(type, name, private, defaultValue)
#endif

#ifndef DEFINE_PROPERTY_NO_COMPARE_PROTECTED_SET
#define DEFINE_PROPERTY_NO_COMPARE_PROTECTED_SET(type, name, defaultValue) _DEFINE_PROPERTY_NO_COMPARE_SCOPED_ACCESS_SET(type, name, protected, defaultValue)
#endif

#endif // __MVVM_CPPWINRT_PROPERTY_MACROS_H_INCLUDED
