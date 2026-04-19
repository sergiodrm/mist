#pragma once

#include <thread>

namespace Mist
{
	typedef uint32_t ThreadId;

	class ThisThread
	{
	public:
		static void YieldTask() { std::this_thread::yield(); }
		static void SleepFor(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
		static bool IsMainThread();
		static void SetMainThread();
		static ThreadId GetId();
	private:
		static ThreadId s_mainThreadId;
	};

	// basic wrapper for std thread
	class Thread
	{
	public:

		Thread() = default;
		template <class Fn, class ... Args>
		explicit Thread(Fn&& fn, Args&& ... args);

		Thread(const Thread&) = delete;
		Thread(Thread&&) = delete;
		Thread& operator=(const Thread&) = delete;
		Thread& operator=(Thread&&) = delete;

		void Join();
		bool IsJoinable() const;
		void Swap(Thread& other);
		void Detach();

	private:
		std::thread m_thread;
	};

	template<class Fn, class ...Args>
	inline Thread::Thread(Fn&& fn, Args && ...args)
		: m_thread(std::forward<Fn>(fn), std::forward<Args>(args)...)
	{ }

	inline void Thread::Join() { m_thread.join(); }
	inline bool Thread::IsJoinable() const { return m_thread.joinable(); }
	inline void Thread::Swap(Thread& other) { m_thread.swap(other.m_thread); }
	inline void Thread::Detach() { m_thread.detach(); }
}