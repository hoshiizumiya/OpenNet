#pragma once
#include <functional>
#include <vector>
#include <utility>

namespace mvvm
{
    // 注册“如何解绑”的回调；析构或 clear() 时顺序执行。
    struct SubscriptionTracker
    {
        void Track(std::function<void()> unbind) { m_unBinders.emplace_back(std::move(unbind)); }

        // 支持链式写法：bag += []{ ...remove... };
        SubscriptionTracker& operator+=(std::function<void()> unbind) { Track(std::move(unbind)); return *this; }

        void Clear() noexcept
        {
            for (auto& f : m_unBinders)
            {
                try { f(); }
                catch (...) { /* swallow */ }
            }
            m_unBinders.clear();
        }

        ~SubscriptionTracker() { Clear(); }

    private:
        std::vector<std::function<void()>> m_unBinders;
    };
}
