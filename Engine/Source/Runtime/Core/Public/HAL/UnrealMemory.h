// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Sizes.

enum 
{ 
	// Default allocator alignment. If the default is specified, the allocator applies to engine rules.
	// Blocks >= 16 bytes will be 16-byte-aligned, Blocks < 16 will be 8-byte aligned. If the allocator does
	// not support allocation alignment, the alignment will be ignored.
	DEFAULT_ALIGNMENT = 0,

	// Minimum allocator alignment
	MIN_ALIGNMENT = 8,
}; 

/*-----------------------------------------------------------------------------
	FMemory.
-----------------------------------------------------------------------------*/

struct CORE_API FMemory
{
	/** @name Memory functions (wrapper for FPlatformMemory) */

	static FORCEINLINE void* Memmove( void* Dest, const void* Src, SIZE_T Count )
	{
		return FPlatformMemory::Memmove( Dest, Src, Count );
	}

	static FORCEINLINE int32 Memcmp( const void* Buf1, const void* Buf2, SIZE_T Count )
	{
		return FPlatformMemory::Memcmp( Buf1, Buf2, Count );
	}

	static FORCEINLINE void* Memset(void* Dest, uint8 Char, SIZE_T Count)
	{
		return FPlatformMemory::Memset( Dest, Char, Count );
	}

	template< class T > 
	static FORCEINLINE void Memset( T& Src, uint8 ValueToSet )
	{
		static_assert( !TIsPointerType<T>::Value, "For pointers use the three parameters function");
		Memset( &Src, ValueToSet, sizeof( T ) );
	}

	static FORCEINLINE void* Memzero(void* Dest, SIZE_T Count)
	{
		return FPlatformMemory::Memzero( Dest, Count );
	}

	template< class T > 
	static FORCEINLINE void Memzero( T& Src )
	{
		static_assert( !TIsPointerType<T>::Value, "For pointers use the two parameters function");
		Memzero( &Src, sizeof( T ) );
	}

	static FORCEINLINE void* Memcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::Memcpy(Dest,Src,Count);
	}

	template< class T > 
	static FORCEINLINE void Memcpy( T& Dest, const T& Src )
	{
		static_assert( !TIsPointerType<T>::Value, "For pointers use the three parameters function");
		Memcpy( &Dest, &Src, sizeof( T ) );
	}

	static FORCEINLINE void* BigBlockMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::BigBlockMemcpy(Dest,Src,Count);
	}

	static FORCEINLINE void* StreamingMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::StreamingMemcpy(Dest,Src,Count);
	}

	static FORCEINLINE void Memswap( void* Ptr1, void* Ptr2, SIZE_T Size )
	{
		FPlatformMemory::Memswap(Ptr1,Ptr2,Size);
	}

	template< class T >
	DEPRECATED(4.8, "Please use Memset.")
	static FORCEINLINE void MemSet( T& Src, uint8 ValueToSet )
	{
		static_assert( !TIsPointerType<T>::Value, "For pointers use the three parameters function");
		FMemory::Memset( &Src, ValueToSet, sizeof( T ) );
	}

	template< class T > 
	DEPRECATED(4.8, "Please use Memzero.")
	static FORCEINLINE void MemZero( T& Src )
	{
		static_assert( !TIsPointerType<T>::Value, "For pointers use the two parameters function");
		FMemory::Memset( &Src, 0, sizeof( T ) );
	}

	template< class T >
	DEPRECATED(4.8, "Please use Memcpy.")
	static FORCEINLINE void MemCopy( T& Dest, const T& Src )
	{
		static_assert( !TIsPointerType<T>::Value, "For pointers use the three parameters function");
		FMemory::Memcpy( &Dest, &Src, sizeof( T ) );
	}

	//
	// C style memory allocation stubs that fall back to C runtime
	//
	static FORCEINLINE void* SystemMalloc(SIZE_T Size)
	{
		return ::malloc(Size);
	}

	static FORCEINLINE void SystemFree(void* Ptr)
	{
		::free(Ptr);
	}

	//
	// C style memory allocation stubs.
	//

	static void* Malloc( SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT );
	static void* Realloc( void* Original, SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT );
	static void Free( void* Original );

	static SIZE_T GetAllocSize( void* Original );

	/**
	 * A helper function that will perform a series of random heap allocations to test
	 * the internal validity of the heap. Note, this function will "leak" memory, but another call
	 * will clean up previously allocated blocks before returning. This will help to A/B testing
	 * where you call it in a good state, do something to corrupt memory, then call this again
	 * and hopefully freeing some pointers will trigger a crash.
	 */
	static void TestMemory();
};

