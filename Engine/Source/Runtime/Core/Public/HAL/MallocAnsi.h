// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#if _MSC_VER || PLATFORM_MAC
	#define USE_ALIGNED_MALLOC 1
#else
	//@todo gcc: this should be implemented more elegantly on other platforms
	#define USE_ALIGNED_MALLOC 0
#endif

#if PLATFORM_IOS
	#include "mach/mach.h"
#endif


//
// ANSI C memory allocator.
//
class FMallocAnsi
	: public FMalloc
{
	
public:
	/**
	 * Constructor enabling low fragmentation heap on platforms supporting it.
	 */
	FMallocAnsi()
	{
#if PLATFORM_WINDOWS
		// Enable low fragmentation heap - http://msdn2.microsoft.com/en-US/library/aa366750.aspx
		intptr_t	CrtHeapHandle	= _get_heap_handle();
		ULONG		EnableLFH		= 2;
		HeapSetInformation( (PVOID)CrtHeapHandle, HeapCompatibilityInformation, &EnableLFH, sizeof(EnableLFH) );
#endif
	}

	// FMalloc interface.
	virtual void* Malloc( SIZE_T Size, uint32 Alignment ) override
	{
		IncrementTotalMallocCalls();
		Alignment = FMath::Max(Size >= 16 ? (uint32)16 : (uint32)8, Alignment);

#if USE_ALIGNED_MALLOC
		void* Result = _aligned_malloc( Size, Alignment );
#else
		void* Ptr = malloc( Size + Alignment + sizeof(void*) + sizeof(SIZE_T) );
		check(Ptr);
		void* Result = Align( (uint8*)Ptr + sizeof(void*) + sizeof(SIZE_T), Alignment );
		*((void**)( (uint8*)Result - sizeof(void*)					))	= Ptr;
		*((SIZE_T*)( (uint8*)Result - sizeof(void*) - sizeof(SIZE_T)	))	= Size;
#endif

		if (Result == nullptr)
		{
			FPlatformMemory::OnOutOfMemory(Size, Alignment);
		}
		return Result;
	}

	virtual void* Realloc( void* Ptr, SIZE_T NewSize, uint32 Alignment ) override
	{
		IncrementTotalReallocCalls();
		void* Result;
		Alignment = FMath::Max(NewSize >= 16 ? (uint32)16 : (uint32)8, Alignment);

#if USE_ALIGNED_MALLOC
		if( Ptr && NewSize )
		{
			Result = _aligned_realloc( Ptr, NewSize, Alignment );
		}
		else if( Ptr == nullptr )
		{
			Result = _aligned_malloc( NewSize, Alignment );
		}
		else
		{
			_aligned_free( Ptr );
			Result = nullptr;
		}
#else
		if( Ptr && NewSize )
		{
			// Can't use realloc as it might screw with alignment.
			Result = Malloc( NewSize, Alignment );
			SIZE_T PtrSize = 0;
			GetAllocationSize(Ptr,PtrSize);
			FMemory::Memcpy( Result, Ptr, FMath::Min(NewSize, PtrSize ) );
			Free( Ptr );
		}
		else if( Ptr == nullptr )
		{
			Result = Malloc( NewSize, Alignment);
		}
		else
		{
			free( *((void**)((uint8*)Ptr-sizeof(void*))) );
			Result = nullptr;
		}
#endif
		if (Result == nullptr && NewSize != 0)
		{
			FPlatformMemory::OnOutOfMemory(NewSize, Alignment);
		}

		return Result;
	}

	virtual void Free( void* Ptr ) override
	{
		IncrementTotalFreeCalls();
#if USE_ALIGNED_MALLOC
		_aligned_free( Ptr );
#else
		if( Ptr )
		{
			free( *((void**)((uint8*)Ptr-sizeof(void*))) );
		}
#endif
	}

	virtual bool GetAllocationSize(void *Original, SIZE_T &SizeOut) override
	{
		if (!Original)
		{
			return false;
		}
#if	USE_ALIGNED_MALLOC
		SizeOut = _aligned_msize(Original,16,0); // Assumes alignment of 16
#else
		SizeOut = *((SIZE_T*)( (uint8*)Original - sizeof(void*) - sizeof(SIZE_T)));	
#endif // USE_ALIGNED_MALLOC
		return true;
	}

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 *
	 * @return true as we're using system allocator
	 */
	virtual bool IsInternallyThreadSafe() const override
	{
#if PLATFORM_MAC
			return true;
#elif PLATFORM_IOS
			return true;
#else
			return false;
#endif
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual bool ValidateHeap() override
	{
#if PLATFORM_WINDOWS
		int32 Result = _heapchk();
		check(Result != _HEAPBADBEGIN);
		check(Result != _HEAPBADNODE);
		check(Result != _HEAPBADPTR);
		check(Result != _HEAPEMPTY);
		check(Result == _HEAPOK);
#else
		return true;
#endif
		return true;
	}

	virtual const TCHAR* GetDescriptiveName() override { return TEXT("ANSI"); }
};
