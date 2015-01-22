// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LockFreeList.h"

/** Thread safe, lock free pooling allocator of fixed size blocks that never returns free space until program shutdown. **/
template<int32 SIZE, typename TTrackingCounter = FNoopCounter>
class TLockFreeFixedSizeAllocator	// alignment isn't handled, assumes FMemory::Malloc will work
{
public:
	/** Destructor, returns all memory via FMemory::Free **/
	~TLockFreeFixedSizeAllocator()
	{
		check(!NumUsed.GetValue());
		while (void* Mem = FreeList.Pop())
		{
			FMemory::Free(Mem);
			NumFree.Decrement();
		}
		check(!NumFree.GetValue());
	}
	/** Returns a memory block of size SIZE **/
	void* Allocate()
	{
		NumUsed.Increment();
		void *Memory = FreeList.Pop();
		if (Memory)
		{
			NumFree.Decrement();
		}
		else
		{
			Memory = FMemory::Malloc(SIZE);
		}
		return Memory;
	}
	/** Given a memory block (obtained from Allocate) put it on the free list for future use. **/
	void Free(void *Item)
	{
		NumUsed.Decrement();
		FreeList.Push(Item);
		NumFree.Increment();
	}

	const TTrackingCounter& GetNumUsed() const
	{
		return NumUsed;
	}

	const TTrackingCounter& GetNumFree() const
	{
		return NumFree;
	}

private:
	/** Lock free list of free memory blocks **/
	TLockFreePointerList<void>		FreeList;

	/** Total number of blocks outstanding and not in the free list **/
	TTrackingCounter	NumUsed; 
	/** Total number of blocks in the free list **/
	TTrackingCounter	NumFree;
};



/** Thread safe, lock free pooling allocator of memory for instances of T. Never returns free space until program shutdown. **/
template<class T>
class TLockFreeClassAllocator : private TLockFreeFixedSizeAllocator<sizeof(T)>
{
public:
	/** Returns a memory block of size sizeof(T) **/
	void* Allocate()
	{
		return TLockFreeFixedSizeAllocator<sizeof(T)>::Allocate();
	}
	/** Returns a new T using the default contructor **/
	T* New()
	{
		return new (Allocate()) T();
	}
	/** Calls a destructor on Item and returns the memory to the free list for recycling. **/
	void Free(T *Item)
	{
		Item->~T();
		TLockFreeFixedSizeAllocator<sizeof(T)>::Free(Item);
	}
};
