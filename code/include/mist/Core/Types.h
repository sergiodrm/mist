#pragma once

#include <stdarg.h>

namespace Mist
{
	typedef char byte;
	typedef unsigned char uint8_t;
	typedef unsigned short int uint16_t;
	typedef unsigned int uint32_t;
	typedef unsigned long int uint64_t;
	typedef char int8_t;
	typedef short int int16_t;
	typedef int int32_t;
	typedef long int int64_t;

	template <uint32_t Size>
	class tFixedString
	{
	public:
		typedef tFixedString<Size> ThisType;

		tFixedString() = default;
		~tFixedString() = default;

		const char* Str() const { return m_string; }
		uint32_t Length() const { return strlen(m_string); }

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
			return *this;
		}
		ThisType& operator=(ThisType&& other)
		{
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