#pragma once

#include <mutex>

namespace Mist
{
	// basic wrapper for std mutex
	class Mutex
	{
	public:
		Mutex() = default;

		Mutex(const Mutex&) = delete;
		Mutex(Mutex&&) = delete;
		Mutex& operator=(const Mutex&) = delete;
		Mutex& operator=(Mutex&&) = delete;

		void Lock();
		void Unlock();
		bool TryLock();

	private:
		std::mutex m_mutex;
	};

	inline void Mutex::Lock() { m_mutex.lock(); }
	inline void Mutex::Unlock() { m_mutex.unlock(); }
	inline bool Mutex::TryLock() { return m_mutex.try_lock(); }

    class GuardMutex
    {
    public:
        GuardMutex(Mutex& mutex);
        ~GuardMutex();
    private:
        Mutex& m_mutex;
    };

    inline GuardMutex::GuardMutex(Mutex& mutex) : m_mutex(mutex) { m_mutex.Lock(); }
    inline GuardMutex::~GuardMutex() { m_mutex.Unlock(); }
}