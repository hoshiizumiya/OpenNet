#pragma once
// AsyncMutex - C++/WinRT port of CommunityToolkit AsyncMutex
// An async mutex implementation using winrt::slim_mutex/event for use in WinRT async contexts

#include <mutex>
#include <queue>
#include <functional>
#include <winrt/Windows.Foundation.h>

namespace OpenNet::UI::Xaml::Media::Helpers
{
	/// <summary>
	/// A simple async-compatible mutex for use in fire_and_forget / IAsyncAction contexts.
	/// Uses a queue + event pattern since C++/WinRT doesn't have SemaphoreSlim.
	/// For simplicity, this implementation uses std::mutex + coroutine resumption.
	/// </summary>
	class AsyncMutex
	{
	public:
		AsyncMutex() = default;
		AsyncMutex(const AsyncMutex&) = delete;
		AsyncMutex& operator=(const AsyncMutex&) = delete;

		/// <summary>
		/// RAII lock guard that releases the mutex on destruction
		/// </summary>
		class Lock
		{
		public:
			Lock(AsyncMutex& owner) : m_owner(owner) {}
			~Lock() { m_owner.Release(); }
			Lock(const Lock&) = delete;
			Lock& operator=(const Lock&) = delete;
			Lock(Lock&& other) noexcept : m_owner(other.m_owner) {}

		private:
			AsyncMutex& m_owner;
		};

		/// <summary>
		/// Acquires the mutex. In the simple case, returns immediately.
		/// If contended, suspends the coroutine until the mutex is available.
		/// </summary>
		winrt::Windows::Foundation::IAsyncAction LockAsync()
		{
			bool acquired = false;
			{
				std::lock_guard guard(m_mutex);
				if (!m_locked)
				{
					m_locked = true;
					acquired = true;
				}
			}

			if (!acquired)
			{
				winrt::handle event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
				{
					std::lock_guard guard(m_mutex);
					if (!m_locked)
					{
						m_locked = true;
						acquired = true;
					}
					else
					{
						m_waiters.push(event.get());
					}
				}

				if (!acquired)
				{
					co_await winrt::resume_on_signal(event.get());
				}
			}

			co_return;
		}

		/// <summary>
		/// Creates a RAII lock. Usage:
		///   auto lock = co_await mutex.ScopedLockAsync();
		///   // ... protected section ...
		///   // lock released automatically
		/// </summary>
		/// Note: Due to C++ coroutine limitations, prefer using LockAsync() + Release() manually,
		/// or use the synchronous TryLock() for non-async contexts.

		/// <summary>
		/// Synchronous try-lock for non-async contexts
		/// </summary>
		bool TryLock()
		{
			std::lock_guard guard(m_mutex);
			if (!m_locked)
			{
				m_locked = true;
				return true;
			}
			return false;
		}

		/// <summary>
		/// Releases the mutex, signaling the next waiter if any
		/// </summary>
		void Release()
		{
			std::lock_guard guard(m_mutex);
			if (!m_waiters.empty())
			{
				HANDLE next = m_waiters.front();
				m_waiters.pop();
				SetEvent(next);
			}
			else
			{
				m_locked = false;
			}
		}

	private:
		std::mutex m_mutex;
		bool m_locked = false;
		std::queue<HANDLE> m_waiters;
	};
}
