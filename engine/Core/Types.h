#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <cstring>
#include <string.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <vcruntime_string.h>
#include "Core/SystemMemory.h"
#include "Core/Debug.h"

#define DELETE_COPY_CONSTRUCTORS(_type) \
	_type(const _type&) = delete; \
	_type(_type&&) = delete; \
	_type& operator=(const _type&) = delete; \
	_type& operator=(_type&&) = delete

#define ZeroMem(ptr, size) memset(ptr, 0, size)

namespace Mist
{
	template <typename T>
	using tDynArray = std::vector<T, Mist::tStdAllocator<T>>;
	template <typename T, size_t N>
	using tArray = std::array<T, N>;
	template <typename Key_t, typename Value_t, typename Hasher_t = std::hash<Key_t>, typename EqualTo = std::equal_to<Key_t>>
	using tMap = std::unordered_map<Key_t, Value_t, Hasher_t, EqualTo, Mist::tStdAllocator<std::pair<const Key_t, Value_t>>>;
	using tString = std::basic_string<char, std::char_traits<char>, Mist::tStdAllocator<char>>;

	typedef uint16_t index_t;
	typedef uint32_t lindex_t;

	enum {index_invalid = UINT16_MAX};

	template <typename T>
	void CopyDynArray(tDynArray<T>& dst, const std::vector<T>& src)
	{
		size_t size = src.size();
		dst.resize(size);
		for (size_t i = 0; i < size; ++i)
			dst[i] = src[i];
	}

	template <typename T>
	inline void HashCombine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x933779b9 + (seed << 6) + (seed >> 2);
	}

	template <uint32_t Size>
	class tFixedString
	{
	public:
		typedef tFixedString<Size> ThisType;

		tFixedString() : m_string{0}
		{}

		tFixedString(const char* str)
		{
			Set(str);
		}

		~tFixedString() = default;

		tFixedString(const tFixedString& other)
		{
			*this = other;
		}

		ThisType& operator=(const char* str)
		{
			check(strlen(str) < Size);
			strcpy_s(m_string, str);
			return *this;
		}

		const char* CStr() const { return m_string; }
		uint32_t Length() const { return (uint32_t)strlen(m_string); }
		bool IsEmpty() const { return !*m_string; }

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
			//memset(Data, 0, sizeof(T) * N);
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

		const T& GetFromLatest(uint32_t i) const { return Get(Index - (i+1)); }
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

		const T& operator[](uint32_t index) const { check(index < PushIndex); return Data[index]; }
		T& operator[](uint32_t index) { check(index < PushIndex); return Data[index]; }

		inline void Resize(uint32_t newSize)
		{
			check(newSize <= Size);
			PushIndex = PushIndex > newSize ? PushIndex : newSize;
		}

		inline uint32_t GetSize() const { return PushIndex; }
		inline bool IsEmpty() const { return !GetSize(); }
		inline constexpr uint32_t GetCapacity() const { return Size; }
		inline void Clear() { PushIndex = 0; }
		inline void Clear(const T& clearValue)
		{
			for (uint32_t i = 0; i < Size; ++i)
			{
				Data[i] = clearValue;
				Data[i].~T();
			}
			Clear();
		}

		void Push(const T& value)
		{
			check(PushIndex < Size);
			Data[PushIndex++] = value;
		}

		void Push()
		{
			check(PushIndex < Size);
			new(&Data[PushIndex++])T();
		}

		void Pop() { check(PushIndex > 0); --PushIndex; }

		inline const T& GetBack() const { check(PushIndex); return Data[PushIndex - 1]; }
		inline T& GetBack() { check(PushIndex); return Data[PushIndex - 1]; }

		inline const T* GetData() const { return Data; }
		inline T* GetData() { return Data; }

		void Swap(index_t item1, index_t item2)
		{
			check(item1 < PushIndex && item2 < PushIndex);
			T temp = Data[item1];
			Data[item1] = Data[item2];
			Data[item2] = temp;
		}


	private:
		T Data[Size];
		uint32_t PushIndex = 0;
	};

	template <typename DataType, typename IndexType = uint16_t>
	struct tSpan
	{
		DataType* Data = nullptr;
		IndexType Count = 0;

		inline const DataType& operator[](IndexType index) const { check(index < Count); return Data[index]; }
		inline DataType& operator[](IndexType index) { check(index < Count); return Data[index]; }
	};

	template <typename DataType, typename IndexType = uint16_t>
	class tFixedHeapArray
	{
		typedef tFixedHeapArray<DataType, IndexType> ThisType;
	public:

		tFixedHeapArray()
			: m_data(nullptr), m_index(0), m_count(0) {}
		tFixedHeapArray(IndexType count)
			: m_data(nullptr), m_index(0), m_count(0)
		{
			Allocate(count);
		}
		~tFixedHeapArray()
		{
			Delete();
		}

		void Allocate(IndexType count)
		{
			check(!m_data && !m_index && !m_count);
			m_count = count;
			m_data = (DataType*)_malloc(count * sizeof(DataType));
		}

		void Delete()
		{
			Clear();
			if (m_data)
			{
				Mist::Free(m_data);
				Invalidate();
			}
		}

		void Clear()
		{
			for (IndexType i = 0; i < m_index; ++i)
			{
				m_data[i].~DataType();
			}
			m_index = 0;
		}

		void Push()
		{
			check(m_data && m_index < m_count);
			new (&m_data[m_index++]) DataType();
		}

		void Push(const DataType& value)
		{
			check(m_data && m_index < m_count);
			m_data[m_index++] = value;
		}

		void Pop()
		{
			check(m_data && m_index);
			m_data[m_index--].~DataType();
		}

		void Resize(uint32_t count)
		{
			check(count <= m_count);
			if (m_index == count) 
				return;
			if (count < m_index)
			{
				for (IndexType i = count; i < m_index; ++i)
					m_data[i].~DataType();
			}
			else
			{
				for (IndexType i = m_index; i < count; ++i)
					new(&m_data[i])DataType();
			}

			m_index = count;
		}

		DataType& Back()
		{
			check(m_index > 0);
			return m_data[m_index - 1];
		}

		const DataType& Back() const { return const_cast<ThisType*>(this)->Back(); }

		inline const DataType* GetData() const { return m_data; }
		inline DataType* GetData() { return m_data; }
		inline IndexType GetSize() const { return m_index; }
		inline IndexType GetReservedSize() const { return m_count; }
		inline bool IsEmpty() const { return m_index == 0; }

		inline DataType& operator[](IndexType index) { check(m_data && index < m_index); return m_data[index]; }
		inline const DataType& operator[](IndexType index) const { check(m_data && index < m_index); return m_data[index]; }

		[[nodiscard]] ThisType Move()
		{
			ThisType other;
			other.m_data = m_data;
			other.m_index = m_index;
			other.m_count = m_count;
			Invalidate();
			return other;
		}

	protected:
		void Invalidate()
        {
            m_data = nullptr;
            m_count = 0;
            m_index = 0;
		}

	private:
		DataType* m_data;
		IndexType m_index;
		IndexType m_count;
	};


	template <typename T, index_t N>
	struct tStackTree
	{
		struct tItem
		{
			index_t DataIndex = index_invalid;
			index_t Parent = index_invalid;
			index_t Child = index_invalid;
			index_t Sibling = index_invalid;
		};

		index_t Current = index_invalid;
		tStaticArray<tItem, N> Items;
		tStaticArray<T, N> Data;

		void Push(const T& v)
		{
			// Push new data
			Data.Push(v);
			index_t dataIndex = Data.GetSize() - 1;
			// Push new item to stack
			Items.Push();
			index_t newItem = Items.GetSize() - 1;
			tItem& item = Items[newItem];
			item.DataIndex = dataIndex;
			item.Parent = index_invalid;
			item.Child = index_invalid;
			item.Sibling = index_invalid;
			// Connect new item to tree
			if (Current != index_invalid)
			{
				// Add as child of Current item and connect with its siblings
				item.Parent = Current;
				tItem& parent = Items[Current];
				index_t lastChild = parent.Child;
				for (lastChild; lastChild != index_invalid && (Items[lastChild].Sibling != index_invalid); lastChild = Items[lastChild].Sibling);
				if (lastChild != index_invalid)
					Items[lastChild].Sibling = newItem;
				else
					parent.Child = newItem;
			}
			else
			{
				// No Current, so this is a "base" stack item. Connect to others "base" items.
				index_t sibling = 0;
				for (sibling; sibling < Items.GetSize() && (Items[sibling].Parent != index_invalid || sibling == newItem); ++sibling);
				if (sibling < Items.GetSize())
				{
					for (sibling; Items[sibling].Sibling != index_invalid; sibling = Items[sibling].Sibling);
					check(sibling < Items.GetSize());
					Items[sibling].Sibling = newItem;
				}
			}
			// Last item added is this.
			Current = newItem;
		}

		void Pop()
		{
			check(Current != index_invalid);
			Current = Items[Current].Parent;
		}

		const T& GetCurrent() const { check(Current != index_invalid); return Data[Items[Current].DataIndex]; }
		T& GetCurrent() { check(Current != index_invalid); return Data[Items[Current].DataIndex]; }

		void Reset()
		{
			check(Current == index_invalid && "Stack tree is not closed! There is a call to Push method without its Pop.");
			Items.Clear();
			Items.Resize(0);
			Data.Clear();
			Data.Resize(0);
		}
	};


	// return number of elements in a static array
	template <typename T, uint32_t N>
	inline constexpr uint32_t CountOf(const T(&)[N])
	{
		return N;
	}

	// return size in bytes of a static array
	template <typename T, uint32_t N>
	inline constexpr uint32_t SizeOf(const T(&)[N])
	{
		return N * sizeof(T);
	}

	bool WildStrcmp(const char* wild, const char* str);
	bool WildStricmp(const char* wild, const char* str);

	template <typename Type>
	inline void Swap(Type& t0, Type& t1)
	{
		Type t = t0;
		t0 = t1;
		t1 = t;
	}
}