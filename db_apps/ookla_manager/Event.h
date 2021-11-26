/*
 * This software is licensed under the Public Domain.
 *
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace tools
{

    class Event
    {
    private:
        bool m_bFlag;
        mutable std::mutex m_mutex;
        mutable std::condition_variable m_condition;

    public:
        inline Event() : m_bFlag(false) {}

        inline void Wait() const
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [&]() -> bool { return m_bFlag; });
        }

        template <typename R, typename P>
        bool Wait(const std::chrono::duration<R, P> &crRelTime) const
        {
            std::unique_lock lock(m_mutex);
            if (!m_condition.wait_for(lock, crRelTime, [&]() -> bool { return m_bFlag; }))
                return false;
            return true;
        }

        inline bool Signal()
        {
            bool bWasSignalled;
            m_mutex.lock();
            bWasSignalled = m_bFlag;
            m_bFlag = true;
            m_mutex.unlock();
            m_condition.notify_all();
            return bWasSignalled == false;
        }

        inline bool Reset()
        {
            bool bWasSignalled;
            m_mutex.lock();
            bWasSignalled = m_bFlag;
            m_bFlag = false;
            m_mutex.unlock();
            return bWasSignalled;
        }

        inline bool IsSet() const { return m_bFlag; }
    };

} // namespace tools