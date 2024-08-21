#pragma once

#include <stdarg.h>
#include <stdint.h>

#define DELETE_COPY_CONSTRUCTORS(_type) \
	_type(const _type&) = delete; \
	_type(_type&&) = delete; \
	_type& operator=(const _type&) = delete; \
	_type& operator=(_type&&) = delete

namespace Mist
{

	template <uint32_t Size>
	class tFixedString
	{
	public:
		typedef tFixedString<Size> ThisType;

		tFixedString() : m_string{0}
		{}

		~tFixedString() = default;

		tFixedString(const tFixedString& other)
		{
			*this = other;
		}

		const char* CStr() const { return m_string; }
		uint32_t Length() const { return (uint32_t)strlen(m_string); }

		void Fmt(const char* fmt, ...)
		{
			va_list va;
			va_start(va, fmt);
			vsprintf_s(m_string, fmt, va);
			va_end(va);
		}

		void Set(const char* str)
		{
			strcpy_s(m_string, str);
		}

		ThisType& operator=(const ThisType& other)
		{
			strcpy_s(m_string, other.m_string);
			return *this;
		}

		ThisType& operator=(ThisType&& other)
		{
			strcpy_s(m_string, other.m_string);
			return *this;
		}

		bool operator==(const ThisType& other) const
		{
			return !strcmp(m_string, other.m_string);
		}

		bool operator==(const char* str) const
		{
			return !strcmp(m_string, str);
		}

		const char& operator[](uint32_t index) const { return m_string[index]; }
		char& operator[](uint32_t index) { return m_string[index]; }

	private:
		char m_string[Size];
	};

	template <typename T, uint32_t N>
	struct tCircularBuffer
	{
		tCircularBuffer() : Index(0)
		{
			memset(Data, 0, sizeof(T) * N);
		}

		void Push(const T& value)
		{
			Data[Index] = value;
			Index = GetIndex(Index + 1);
		}

		const T& Get(uint32_t i) const
		{
			return Data[GetIndex(i)];
		}

		const T& GetLast() const { return Get(Index - 1); }
		uint32_t GetIndex(uint32_t i) const { return i % N; }
		constexpr uint32_t GetCount() const { return N; }

	private:
		T Data[N];
		uint32_t Index;
	};
}