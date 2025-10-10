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
#ifndef __MVVM_CPPWINRT_VIEW_MODEL_H_INCLUDED
#define __MVVM_CPPWINRT_VIEW_MODEL_H_INCLUDED

#include "view_model_base.h"
#include "subscription_tracker.h"

#include <winrt/Microsoft.UI.Dispatching.h>


namespace mvvm
{
    template <typename Derived>
    struct __declspec(empty_bases)ViewModel
        : ViewModelBase<Derived>
    {
        friend typename Derived;

        ViewModel(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
        {
            if (dispatcher)
            {
                m_dispatcher = dispatcher;
            }
            else
            {
                m_dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

                if (!m_dispatcher)
                {
                    throw winrt::hresult_wrong_thread(L"ViewModels must be instantiated on a UI thread."sv);
                }
            }
        }

        winrt::Microsoft::UI::Dispatching::DispatcherQueue Dispatcher() const { return m_dispatcher; }

        winrt::Microsoft::UI::Dispatching::DispatcherQueue GetDispatcherOverride() { return m_dispatcher; }

        void RegisterForAutoCleanup(winrt::Windows::Foundation::IInspectable const& obj)
        {
            if (obj) m_cleanupEntries.push_back({ winrt::make_weak(obj), {} });
        }

        template<typename F>
        void RegisterForAutoCleanup(winrt::Windows::Foundation::IInspectable const& obj, F&& clearAction)
        {
            if (obj)
                m_cleanupEntries.push_back({ winrt::make_weak(obj), std::forward<F>(clearAction) });
        }

        void FrameworkCleanup() noexcept
        {
            // 取消正在执行的命令
            for (auto it = m_cleanupEntries.begin(); it != m_cleanupEntries.end(); )
            {
                if (auto obj = it->object.get())
                {
                    if (auto cmdc = obj.try_as<winrt::Mvvm::Framework::Core::ICommandCleanup>())
                        cmdc.Cancel();
                    ++it;
                }
                else it = m_cleanupEntries.erase(it);
            }

            // 解除注册的依赖关系
            for (auto it = m_cleanupEntries.begin(); it != m_cleanupEntries.end(); )
            {
                if (auto obj = it->object.get())
                {
                    if (auto cmdc = obj.try_as<winrt::Mvvm::Framework::Core::ICommandCleanup>())
                    {
                        cmdc.DetachAllDependencies();
                        cmdc.ClearAllSubscribers();
                        cmdc.ResetHandlers();
                    }
                    ++it;
                }
                else it = m_cleanupEntries.erase(it);
            }

            // 置空 VM 上的命令属性（派生类注册的 clear 回调中设置数值）
            auto postClear = [this]()
                {
                    for (auto& e : m_cleanupEntries)
                    {
                        if (e.clear) { try { e.clear(); } catch (...) {} }
                    }
                    m_cleanupEntries.clear();
                };

            if (this->derived().HasThreadAccess())
                postClear();
            else if (auto dq = this->derived().GetDispatcherOverride())
                dq.TryEnqueue(postClear);

            // 清理 VM 自己的依赖广播/校验器
            if constexpr (requires(Derived d) { d.ClearDependencies(); })
            {
                try {
                    this->derived().ClearDependencies();
                }
                catch (...) {}
            }
            if constexpr (requires(Derived d) { d.ClearValidators(); })
            {
                try {
                    this->derived().ClearValidators();
                }
                catch (...) {}
            }

            UnbindAll();
        }

        // 注册解绑所需要的执行的回调
        template<typename F>
        void TrackUnbind(F&& f) { m_subscTracker.Track(std::forward<F>(f)); }

        // 主动清理（供 Reset/析构 等调用）
        // 支持 OnUnbind 扩展点
        void UnbindAll() noexcept
        {
            m_subscTracker.Clear();
            if constexpr (requires(Derived d) { d.OnUnbind(); })
            {
                try {
                    this->derived().OnUnbind();
                }
                catch (...) {}
            }
        }

    private:
        ViewModel() : ViewModel(nullptr)
        {
            // Default constructor is private to ensure that the ViewModel is always constructed with a dispatcher.
            // This prevents issues with UI thread access.
            static_assert(!std::is_same_v<Derived, ViewModel>, "Default constructor is not allowed for ViewModel.");
            static_assert(std::is_base_of_v<ViewModel<Derived>, Derived>, "Derived class must inherit from ViewModel");
        }

    protected:
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        struct CleanupEntry
        {
            winrt::weak_ref<winrt::Windows::Foundation::IInspectable> object;
            std::function<void()> clear; // 由派生类提供：把对应属性设为 nullptr
        };

        std::vector<CleanupEntry> m_cleanupEntries;
        ::mvvm::SubscriptionTracker m_subscTracker;
    };
}

#endif // __MVVM_CPPWINRT_VIEW_MODEL_H_INCLUDED
