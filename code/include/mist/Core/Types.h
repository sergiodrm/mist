#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <cassert>
#include <cstring>
#include <string.h>
#include <vector>
#include <unordered_map>
#include <string>
#include "Core/SystemMemory.h"

#define DELETE_COPY_CONSTRUCTORS(_type) \
	_type(const _type&) = delete; \
	_type(_type&&) = delete; \
	_type& operator=(const _type&) = delete; \
	_type& operator=(_type&&) = delete

namespace Mist
{
	template <typename T>
	using tDynArray = std::vector<T, Mist::tStdAllocator<T>>;
	template <typename T, size_t N>
	using tArray = std::array<T, N>;
	template <typename Key_t, typename Value_t, typename Hasher_t = std::hash<Key_t>, typename EqualTo = std::equal_to<Key_t>>
	using tMap = std::unordered_map<Key_t, Value_t, Hasher_t, EqualTo, Mist::tStdAllocator<std::pair<const Key_t, Value_t>>>;
	using tString = std::basic_string<char, std::char_traits<char>, Mist::tStdAllocator<char>>;

	template <typename T>
	void CopyDynArray(tDynArray<T>& dst, const std::vector<T>& src)
	{
		size_t size = src.size();
		dst.resize(size);
		for (size_t i = 0; i < size; ++i)
			dst[i] = src[i];
	}

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

		ThisType& operator=(const char* str)
		{
			assert(strlen(str) < Size);
			strcpy_s(m_string, str);
			return *this;
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

		const T& GetFromOldest(uint32_t i) const { return Data[GetIndex(Index + 1 + i)]; }

		const T& GetLast() const { return Get(Index - 1); }
		uint32_t GetIndex(uint32_t i) const { return i % N; }
		constexpr uint32_t GetCount() const { return N; }

	private:
		T Data[N];
		uint32_t Index;
	};

	template <typename T, uint32_t Size>
	struct tStaticArray
	{
		typedef tStaticArray<T, Size> tThisType;
	public:
		tStaticArray() = default;
		tStaticArray(const T& defaultValue)
		{
			for (uint32_t i = 0; i < Size; ++i)
				Data[i] = defaultValue;
		}
		tStaticArray(const tThisType& other)
		{
			for (uint32_t i = 0; i < other.GetSize(); ++i)
				Data[i] = other[i];
		}

		const T& operator[](uint32_t index) const { assert(index < PushIndex); return Data[index]; }
		T& operator[](uint32_t index) { assert(index < PushIndex); return Data[index]; }

		inline void Resize(uint32_t newSize)
		{
			assert(newSize <= Size);
			PushIndex = PushIndex > newSize ? PushIndex : newSize;
		}

		inline uint32_t GetSize() const { return PushIndex; }
		inline bool IsEmpty() const { return !GetSize(); }
		inline constexpr uint32_t GetCapacity() const { return Size; }
		inline void Clear() { PushIndex = 0; }
		inline void Clear(const T& clearValue)
		{
			for (uint32_t i = 0; i < Size; ++i)
				Data[Size] = clearValue;
		}

		void Push(const T& value)
		{
			assert(PushIndex < Size);
			Data[PushIndex++] = value;
		}

		void Pop() { assert(PushIndex > 0); --PushIndex; }

		inline const T& GetBack() const { assert(PushIndex); return Data[PushIndex - 1]; }
		inline T& GetBack() { assert(PushIndex); return Data[PushIndex - 1]; }

		inline const T* GetData() const { return Data; }
		inline T* GetData() { return Data; }


	private:
		T Data[Size];
		uint32_t PushIndex = 0;
	};
}