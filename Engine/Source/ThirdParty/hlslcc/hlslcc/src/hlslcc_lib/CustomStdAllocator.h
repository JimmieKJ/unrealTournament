// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// THIRD_PARTY* are macros that aid static analysis of the engine, they won't be defined for hlslcc project
#ifdef __UNREAL__
THIRD_PARTY_INCLUDES_START
#endif // __UNREAL__
	#include <string>
#ifdef __UNREAL__
THIRD_PARTY_INCLUDES_END
#endif // __UNREAL__

// on Linux, use a custom allocator to work around mismatch between STL classes in a precompiled hlslcc and the engine
#ifdef __gnu_linux__

template<typename T>
class FCustomStdAllocator
{
public:

	typedef T value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

	template<typename U>
	struct rebind
	{
		typedef FCustomStdAllocator<U> other;
	};

	inline pointer allocate(
		size_type cnt,
		typename std::allocator<void>::const_pointer = 0)
	{
		return reinterpret_cast<pointer>(malloc(cnt * sizeof (T)));
	}

	inline void deallocate(pointer p, size_type)
	{
		return free(p);
	}

	inline FCustomStdAllocator()
	{
	}

	inline ~FCustomStdAllocator()
	{
	}

	inline FCustomStdAllocator(FCustomStdAllocator const &)
	{
	}

	template<typename U>
	inline  FCustomStdAllocator(FCustomStdAllocator<U> const &)
	{
	}

	inline pointer address(reference r)
	{
		return &r;
	}

	inline const_pointer address(const_reference r)
	{
		return &r;
	}

	inline size_type max_size() const
	{
		return std::numeric_limits<size_type>::max() / sizeof(T);
	}

	inline void construct(pointer p, const T& t)
	{
		new(p) T(t);
	}

	inline void destroy(pointer p)
	{
		p->~T();
	}

	inline bool operator==(FCustomStdAllocator const &)
	{
		return true;
	}

	inline bool operator==(FCustomStdAllocator const &) const
	{
		return true;
	}

	inline bool operator!=(FCustomStdAllocator const &a)
	{
		return !((*this)==(a));
	}

	inline bool operator!=(FCustomStdAllocator const &a) const
	{
		return !((*this)==(a));
	}
};

#else

// Default allocator.
template<typename T>
using FCustomStdAllocator = std::allocator<T>;

#endif // __gnu_linux__

typedef std::basic_string<char, std::char_traits<char>, FCustomStdAllocator<char>> FCustomStdString;

