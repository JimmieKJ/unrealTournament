// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"

#define CHECK_NON_CONCURRENT_ASSUMPTIONS (0)

#define USE_NEW_LOCK_FREE_LISTS (PLATFORM_PS4 || PLATFORM_XBOXONE || PLATFORM_SWITCH)

class FNoncopyable;
class FNoopCounter;
struct FMemory;

/** 
 * Base class for a lock free list of pointers 
 * This class has several different features. Not all methods are supported in all situations.
 * The intention is that derived classes use private inheritance to build a data structure only supports a small meaningful set of operations.
 *
 * A key feature is that lists can be "closed". A closed list cannot be pushed to or popped from. Closure is atomic with push and pop.
 * All operations are threadsafe, except that the constructor is not reentrant.
 */
class FLockFreeVoidPointerListBase : public FNoncopyable
{
public:
	/** Constructor - sets the list to empty and initializes the allocator if it hasn't already been initialized. */
	FLockFreeVoidPointerListBase()
		: Head(nullptr)
	{
		FLinkAllocator::Get(); // construct allocator singleton if it isn't already
	}
	/** Destructor - checks that the list is either empty or closed. Lists should not be destroyed in any other state. */
	~FLockFreeVoidPointerListBase()
	{
		// leak these at shutdown
		//checkLockFreePointerList(Head == nullptr || Head == FLinkAllocator::Get().ClosedLink()); // we do not allow destruction except in the empty or closed state
	}

	/**	
	 *	Push an item onto the head of the list
	 *	@param NewItem, the new item to push on the list, cannot be nullptr
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	void Push(void *NewItem)
	{
		FLink* NewLink = FLinkAllocator::Get().AllocateLink(NewItem);
		NewLink->Link(&Head);
		checkLockFreePointerList(NewLink->Next != FLinkAllocator::Get().ClosedLink()); // it is not permissible to call push on a Closed queue
	}

	/**	
	 *	Push an item onto the head of the list, unless the list is closed
	 *	@param NewItem, the new item to push on the list, cannot be nullptr
	 *	@return true if the item was pushed on the list, false if the list was closed.
	 */
	bool PushIfNotClosed(void *NewItem)
	{
		FLink* NewLink = FLinkAllocator::Get().AllocateLink(NewItem);
		bool bSuccess = NewLink->LinkIfHeadNotEqual(&Head, FLinkAllocator::Get().ClosedLink());
		if (!bSuccess)
		{
			NewLink->Dispose();
		}
		return bSuccess;
	}

	/**	
	 *	Push an item onto the head of the list, opening it first if necessary
	 *	@param NewItem, the new item to push on the list, cannot be nullptr
	 *	@return true if the list needed to be opened first, false if the list was not closed before our push
	 */
	bool ReopenIfClosedAndPush(void *NewItem)
	{
		FLink* NewLink = FLinkAllocator::Get().AllocateLink(NewItem);
		while (1)
		{
			if (NewLink->LinkIfHeadNotEqual(&Head, FLinkAllocator::Get().ClosedLink()))
			{
				return false;
			}
			if (NewLink->ReplaceHeadIfHeadEqual(&Head, FLinkAllocator::Get().ClosedLink()))
			{
				return true;
			}
		}
	}

	/**	
	 *	Pop an item from the list or return nullptr if the list is empty
	 *	@return The popped item, if any
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	void* Pop()
	{
		FLink* Link = FLink::Unlink(&Head);
		if (!Link)
		{
			return nullptr;
		}
		checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink()); // it is not permissible to call pop on a Closed queue
		void* Return = Link->Item;
		Link->Dispose();
		return Return;
	}

	/**	
	 *	Pop an item from the list or return nullptr if the list is empty or closed
	 *	@return The new item, if any
	 */
	void* PopIfNotClosed()
	{
		FLink* Link = FLink::Unlink(&Head, FLinkAllocator::Get().ClosedLink());
		if (!Link)
		{
			return nullptr;
		}
		checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink()); // internal error, this should not be possible
		void* Return = Link->Item;
		Link->Dispose();
		return Return;
	}

	/**	
	 *	Close the list if it is empty
	 *	@return true if this call actively closed the list
	 */
	bool CloseIfEmpty()
	{
		if (FLinkAllocator::Get().ClosedLink()->ReplaceHeadIfHeadEqual(&Head,nullptr))
		{
			return true;
		}
		return false;
	}

	/**	
	 *	If the list is empty, replace it with the other list and null the other list.
	 *	@return true if this call actively closed the list
	 */
	bool ReplaceListIfEmpty(FLockFreeVoidPointerListBase& NotThreadSafeTempListToReplaceWith)
	{
		if (NotThreadSafeTempListToReplaceWith.Head->ReplaceHeadIfHeadEqual(&Head,nullptr))
		{
			NotThreadSafeTempListToReplaceWith.Head = nullptr;
			return true;
		}
		return false;
	}

	/**	
	 *	Pop all items from the list 
	 *	@param Output The array to hold the returned items. Must be empty.
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	template <class ARRAYTYPE, typename ElementType>
	void PopAll(ARRAYTYPE& Output)
	{
		checkLockFreePointerList(!Output.Num());
		FLink* Link = FLink::ReplaceList(&Head);
		while (Link)
		{
			checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink()); // it is not permissible to call PopAll on a Closed queue
			checkLockFreePointerList(Link->Item); // we don't allow nullptr items
			Output.Add((ElementType)(Link->Item));
			FLink* NextLink = Link->Next;
			Link->Dispose();
			Link = NextLink;
		}
	}

	/**	
	 *	Pop all items from the list and atomically close it.
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	template <class ARRAYTYPE, typename ElementType>
	void PopAllAndClose(ARRAYTYPE& Output)
	{
		checkLockFreePointerList(!Output.Num());
		FLink* Link = FLink::ReplaceList(&Head, FLinkAllocator::Get().ClosedLink());
		while (Link)
		{
			checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink()); // we should not pop off the closed link
			checkLockFreePointerList(Link->Item); // we don't allow nullptr items
			Output.Add((ElementType)(Link->Item));
			FLink* NextLink = Link->Next;
			Link->Dispose();
			Link = NextLink;
		}
	}

	/**	
	 *	Check if the list is closed
	 *	@return true if the list is closed.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could open or close the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is only closed once it is lifetime, so a return of true here means it is closed forever.
	 */
	bool IsClosed() const  // caution, if this returns false, that does not mean the list is open!
	{
		FPlatformMisc::MemoryBarrier();
		return Head == FLinkAllocator::Get().ClosedLink();
	}

	/**	
	 *	Check if the list is empty
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	bool IsEmpty() const  
	{
		FPlatformMisc::MemoryBarrier();
		return Head == nullptr;
	}


private:
	/**	Internal single-linked list link structure. */
	struct FLink
	{
		/** Next item in the list or nullptr if we are at the end of the list */
		FLink*				Next;
		/** 
		 * Pointer to the item this list holds. The is useful to the external users of the system. Must not be nullptr for ordinary links. 
		 * Also part of the solution to the ABA problem. nullptr Items are always on the recycling list and NON-nullptr items are not.
		 */
		void*				Item;
		/** Part of the solution to the ABA problem. Links cannot be recycled until no threads hold pointers to them. When the lock count drops to zero, the link is recycled. */
		FThreadSafeCounter	LockCount;
		/** Part of the solution to the ABA problem. Links that are free still need locking, but we need to make sure that links are only added to the free list when they are transitioning to the free list. */
		FThreadSafeCounter	MarkedForDeath;

		/** Constructor, everything is initialized to zero. */
		FLink()
			: Next(nullptr)
			, Item(nullptr)
		{
		}

		/**	
		 *	Link this node into the head of the given list.
		 *	@param HeadPointer; the head of the list
		 *	CAUTION: Not checked here, but linking into a closed list with the routine would accidently and incorrectly open the list
		 */
		void Link(FLink** HeadPointer)
		{
			CheckNotMarkedForDeath();
			while (1)
			{
				FLink*  LocalHeadPointer = LockLink(HeadPointer);
				CheckNotMarkedForDeath();
				SetNextPointer(LocalHeadPointer);
				FLink* ValueWas = (FLink*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)HeadPointer,this,LocalHeadPointer);
				if (ValueWas == LocalHeadPointer)
				{
					if (LocalHeadPointer)
					{
						LocalHeadPointer->Unlock();
					}
					break;
				}
				SetNextPointer(nullptr);
				if (LocalHeadPointer)
				{
					LocalHeadPointer->Unlock();
				}
			}
		}

		/**	
		 *	Link this node into the head of the given list, unless the head is a special, given node
		 *	@param HeadPointer; the head of the list
		 *	@param SpecialClosedLink; special link that is never recycled that indicates a closed list.
		 *	@return true if this node was linked to the head.
		 */
		bool LinkIfHeadNotEqual(FLink** HeadPointer, FLink* SpecialClosedLink)
		{

			checkLockFreePointerList(!Next); // this should not be set yet
			CheckNotMarkedForDeath();
			while (1)
			{
				if (*HeadPointer == SpecialClosedLink)
				{
					CheckNotMarkedForDeath();
					return false;
				}
				FLink*  LocalHeadPointer = LockLink(HeadPointer, SpecialClosedLink);
				CheckNotMarkedForDeath();
				// here if the list turns out to be locked now, the compare and swap will fail anyway
				SetNextPointer(LocalHeadPointer);
				FLink* ValueWas = (FLink*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)HeadPointer,this,LocalHeadPointer);
				if (ValueWas == LocalHeadPointer)
				{
					if (LocalHeadPointer)
					{
						LocalHeadPointer->Unlock();
					}
					break;
				}
				SetNextPointer(nullptr);
				if (LocalHeadPointer)
				{
					LocalHeadPointer->Unlock();
				}
			}
			return true;
		}
		/**	
		 *	If the head is a particular value, then replace it with this node. This is the primitive that is used to close and open lists
		 *	@param HeadPointer; the head of the list
		 *	@param SpecialTestLink; special link that is never recycled that indicates a closed list. Or nullptr in the case that we are using this to open the list
		 *	@return true if this node was linked to the head.
		 *	CAUTION: The test link must nullptr or a special link that is never recycled. The ABA problem is assumed to not occur here.
		 */
		bool ReplaceHeadIfHeadEqual(FLink** HeadPointer, FLink* SpecialTestLink)
		{
			CheckNotMarkedForDeath();
			while (1)
			{
				if (*HeadPointer != SpecialTestLink)
				{
					return false;
				}
				CheckNotMarkedForDeath();
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)HeadPointer,this,SpecialTestLink) == SpecialTestLink)
				{
					break;
				}
			}
			return true;
		}
		/**	
		 *	Pop an item off the list, unless the list is empty or the head is a special, given node
		 *	@param HeadPointer; the head of the list
		 *	@param SpecialClosedLink; special link that is never recycled that indicates a closed list. Can be nullptr for lists that are known to not be closed
		 *	@return The link that was popped off, or nullptr if the list was empty or closed.
		 *	CAUTION: Not checked here, but if the list is closed and SpecialClosedLink is nullptr, you will pop off the closed link, and that usually isn't what you want
		 */
		static FLink* Unlink(FLink** HeadPointer, FLink* SpecialClosedLink = nullptr)
		{
			while (1)
			{
				FLink*  LocalHeadPointer = LockLink(HeadPointer, SpecialClosedLink);
				if (!LocalHeadPointer)
				{
					break;
				}
				FLink* NextLink = LocalHeadPointer->Next;
				FLink* ValueWas = (FLink*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)HeadPointer,NextLink,LocalHeadPointer);
				if (ValueWas == LocalHeadPointer)
				{
					checkLockFreePointerList(NextLink == LocalHeadPointer->Next);
					LocalHeadPointer->SetNextPointer(nullptr); // this is redundant, but we are not given the caller ownership of the other items
					LocalHeadPointer->Unlock(true); // this cannot possibly result in a destruction of the link because we have taken sole possession of it
					LocalHeadPointer->CheckNotMarkedForDeath(); // we are the exclusive owners at this point
					return LocalHeadPointer;
				}
				LocalHeadPointer->Unlock();
			}
			return nullptr;
		}
		/**	
		 *	Replace the list with another list.h Use to either acquire all of the items or acquire all of the items and close the list.
		 *	@param HeadPointer; the head of the list
		 *	@param NewHeadLink; Head of the new list to replace this list with
		 *	@return The original list before we replaced it.
		 *	CAUTION: Not checked here, but if the list is closed this is probably not what you want. 
		 */
		static FLink* ReplaceList(FLink** HeadPointer, FLink* NewHeadLink = nullptr)
		{
			while (1)
			{
				FLink*  LocalHeadPointer = LockLink(HeadPointer);
				if (!LocalHeadPointer && !NewHeadLink)
				{
					// we are asking to replace nullptr with nullptr, this does not require an atomic (or any) operation
					break;
				}
				checkLockFreePointerList(LocalHeadPointer != NewHeadLink); // replacing nullptr with nullptr is ok, but otherwise we cannot be replacing something with itself or we lose determination of who did what.
				FLink* ValueWas = (FLink*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)HeadPointer,NewHeadLink,LocalHeadPointer);
				if (ValueWas == LocalHeadPointer)
				{
					if (LocalHeadPointer)
					{
						LocalHeadPointer->Unlock(true); // this cannot possibly result in a destruction of the link because we have taken sole possession of it
						LocalHeadPointer->CheckNotMarkedForDeath(); // we are the exclusive owners at this point
					}
					return LocalHeadPointer;
				}
				if (LocalHeadPointer)
				{
					LocalHeadPointer->Unlock();
				}
			}
			return nullptr;  
		}

		/**	
		 *	Internal function to safely grab the head pointer and increase the ref count of a link to avoid the ABA problem
		 *	@param HeadPointer; the head of the list
		 *	@param SpecialClosedLink; special link that is never recycled that indicates a closed list. Can be nullptr for lists that are known to not be closed, or if you want to operate on the closed link.
		 *	@return Pointer to the head link, with the ref count increased. Or nullptr if the head is nullptr or SpecialClosedLink
		 *	CAUTION: Not checked here, but if the list is closed this is probably not what you want. 
		 */
		static FLink* LockLink(FLink** HeadPointer, FLink* SpecialClosedLink = nullptr)
		{
			while (1)
			{
				FLink*  LocalHeadPointer = *HeadPointer;
				if (!LocalHeadPointer || LocalHeadPointer == SpecialClosedLink)
				{
					return nullptr;
				}
				LocalHeadPointer->LockCount.Increment();
				FPlatformMisc::MemoryBarrier();
				if (*HeadPointer == LocalHeadPointer)
				{
					return LocalHeadPointer;
				}
				LocalHeadPointer->Unlock();
			}
		}

		void SetNextPointer(FLink* NewNext)
		{
			FLink* LocalNextPointer = Next;
			checkLockFreePointerList(LocalNextPointer != NewNext || !NewNext); // should not be the same, except both can be nullptr
			checkLockFreePointerList(!LocalNextPointer || !NewNext); // one (or both) of them should be nullptr
#if CHECK_NON_CONCURRENT_ASSUMPTIONS
			FLink* ValueWas = (FLink*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Next, NewNext, LocalNextPointer); // not known to need to be interlocked
			checkLockFreePointerList(ValueWas == LocalNextPointer); // this should always work because there should never be contention on setting next
#else
			Next = NewNext;
#endif
			FPlatformMisc::MemoryBarrier();
			if (NewNext)
			{
				CheckNotMarkedForDeath(); // it shouldn't be possible for the node to die while we are setting a non-nullptr next pointer
			}
		}

		// verify that we are not marked for destruction
		void CheckNotMarkedForDeath()
		{
			checkLockFreePointerList(MarkedForDeath.GetValue() == 0); // we don't allow nullptr items
		}

		/**	
		 *	Mark the node for disposal and unlock it
		 */
		void Dispose()
		{
			CheckNotMarkedForDeath();

#if CHECK_NON_CONCURRENT_ASSUMPTIONS
			{
				void *CurrentItem = Item;
				checkLockFreePointerList(CurrentItem); // we don't allow nullptr items
				void* ValueWas = FPlatformAtomics::InterlockedCompareExchangePointer(&Item,nullptr,CurrentItem); // do not need an interlocked operation here
				checkLockFreePointerList(ValueWas == CurrentItem); // there should be no concurrency here
			}

			{
				FLink* LocalNextPointer = Next;
				FLink* ValueWas = (FLink*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Next, nullptr, LocalNextPointer); // not known to need to be interlocked
				checkLockFreePointerList(ValueWas == LocalNextPointer); // this should always work because there should never be contention on setting next
			}
#else
			Item = nullptr;
			Next = nullptr;
#endif
			FPlatformMisc::MemoryBarrier(); // memory barrier in here to make sure item and next are cleared before the link is recycled

			int32 Death = MarkedForDeath.Increment();
			checkLockFreePointerList(Death == 1); // there should be no concurrency here
			Unlock();
		}

		/**	
		 *	Internal function to unlock a link and recycle it if the ref count drops to zero and it hasn't already been recycled.
		 *	This solves the ABA problem by delaying recycling until all pointers to this link are gone.
		 *	@param bShouldNeverCauseAFree - if true, then this should never result in a free if it does, there is a run time error
		 */
		void Unlock(bool bShouldNeverCauseAFree = false);
	};

	/**	
	 *	Class to allocate and recycle links.
	 */
	class FLinkAllocator
	{
	public:

		/**	
		 *	Return a new link.
		 *	@param NewItem "item" pointer of the new node.
		 *	@return New node, ready for use
		 */
		FLink* AllocateLink(void *NewItem)
		{
			checkLockFreePointerList(NewItem); // we don't allow nullptr items
			if (NumUsedLinks.Increment() % 8192 == 1)
			{
				UE_CLOG(0/*MONITOR_LINK_ALLOCATION*/,LogLockFreeList, Log, TEXT("Number of links %d"),NumUsedLinks.GetValue());
			}

			FLink*  NewLink = FLink::Unlink(&FreeLinks);
			if (NewLink)
			{
				NumFreeLinks.Decrement();
			}
			else
			{
				if (NumAllocatedLinks.Increment() % 10 == 1)
				{
					UE_CLOG(0/*MONITOR_LINK_ALLOCATION*/,LogLockFreeList, Log, TEXT("Number of allocated links %d"),NumAllocatedLinks.GetValue());
				}

				NewLink = new FLink();
				NewLink->LockCount.Increment();
			}
			checkLockFreePointerList(!NewLink->Item);
			checkLockFreePointerList(!NewLink->Next);
			checkLockFreePointerList(NewLink->LockCount.GetValue() >= 1);
#if CHECK_NON_CONCURRENT_ASSUMPTIONS
			void* ValueWas = FPlatformAtomics::InterlockedCompareExchangePointer(&NewLink->Item, NewItem, nullptr); // in theory this doesn't need to be interlocked
			checkLockFreePointerList(ValueWas == nullptr);
#else
			NewLink->Item = NewItem;
#endif
			FPlatformMisc::MemoryBarrier();
			return NewLink;
		}
		/**	
		 *	Make a link available for recycling. 
		 *	@param Link link to recycle
		 *	CAUTION: Do not call this directly, it should only be called when the reference count drop to zero.
		 */
		void FreeLink(FLink* Link)
		{
			checkLockFreePointerList(Link != ClosedLink());  // very bad to recycle the special link
			NumUsedLinks.Decrement();
			Link->LockCount.Increment();
			FPlatformMisc::MemoryBarrier();
			Link->Link(&FreeLinks);
			NumFreeLinks.Increment();
		}
		/**	
		 *	Return a pointer to the special closed link
		 *	@return pointer to special closed link
		 */
		FLink* ClosedLink()
		{
			checkLockFreePointerList(nullptr != SpecialClosedLink);
			return SpecialClosedLink;
		}
		/**	
		 *	Singleton access
		 *	@return the singleton for the link allocator
		 */
		static CORE_API FLinkAllocator& Get();

	private:
		/**	
		 *	Constructor zeros the free link list and creates the special closed link
		 *	@return the singleton for the link allocator
		 */
		FLinkAllocator();

		/**	
		 *	Destructor, should only be called when there are no outstanding links. 
		 *	Frees the elements of the free list and frees the special link
		 *	@return the singleton for the link allocator
		 */
		~FLinkAllocator();

		/** Head to a list of free links that can be used for new allocations. */
		FLink*				FreeLinks;

		/** Pointer to the special closed link. It should never be recycled. */
		FLink*				SpecialClosedLink;

		/** Total number of links outstanding and not in the free list */
		FLockFreeListCounter	NumUsedLinks; 
		/** Total number of links in the free list */
		FLockFreeListCounter	NumFreeLinks;
		/** Total number of links allocated */
		FLockFreeListCounter	NumAllocatedLinks; 
	};

	/** Head of the list */
	MS_ALIGN(8) FLink*	Head;
};

/**	
 *	Internal function to unlock a link and recycle it if the ref count drops to zero and it hasn't already been recycled.
 *	This solves the ABA problem by delaying recycling until all pointers to this link are gone.
 *	@param bShouldNeverCauseAFree - if true, then this should never result in a free if it does, there is a run time error
 */
inline void FLockFreeVoidPointerListBase::FLink::Unlock(bool bShouldNeverCauseAFree)
{
	FPlatformMisc::MemoryBarrier();
	checkLockFreePointerList(LockCount.GetValue() > 0);
	if (LockCount.Decrement() == 0)
	{
		checkLockFreePointerList(MarkedForDeath.GetValue() < 2);
		if (MarkedForDeath.Reset())
		{
			checkLockFreePointerList(!bShouldNeverCauseAFree); 
			FLockFreeVoidPointerListBase::FLinkAllocator::Get().FreeLink(this);
		}
	}
}

typedef FLockFreeVoidPointerListBase	FLockFreeVoidPointerListGeneric;

// New lock free lists

template<class T, typename TBundleRecycler, typename TTrackingCounter = FNoopCounter>
class TPointerSet_TLSCacheBase : public FNoncopyable
{
	enum
	{
		NUM_PER_BUNDLE=32,
	};
public:

	TPointerSet_TLSCacheBase()
	{
		check(IsInGameThread());
		TlsSlot = FPlatformTLS::AllocTlsSlot();
		check(FPlatformTLS::IsValidTlsSlot(TlsSlot));
	}
	/** Destructor, leaks all of the memory **/
	~TPointerSet_TLSCacheBase()
	{
		FPlatformTLS::FreeTlsSlot(TlsSlot);
		TlsSlot = 0;
	}

	/**
	 * Allocates a memory block of size SIZE.
	 *
	 * @return Pointer to the allocated memory.
	 * @see Free
	 */
	T* Pop()
	{
		FThreadLocalCache& TLS = GetTLS();

		if (!TLS.PartialBundle)
		{
			if (TLS.FullBundle)
			{
				TLS.PartialBundle = TLS.FullBundle;
				TLS.FullBundle = nullptr;
			}
			else
			{
				TLS.PartialBundle = GlobalFreeListBundles.Pop();
				if (!TLS.PartialBundle)
				{
					TLS.PartialBundle = T::GetBundle(NUM_PER_BUNDLE);
					NumFree.Add(NUM_PER_BUNDLE);
				}
			}
			TLS.NumPartial = NUM_PER_BUNDLE;
		}
		NumUsed.Increment();
		NumFree.Decrement();
		T* Result = TLS.PartialBundle;
		TLS.PartialBundle = (T*)(TLS.PartialBundle->LockFreePointerQueueNext);
		TLS.NumPartial--;
		checkLockFreePointerList(TLS.NumPartial >= 0 && ((!!TLS.NumPartial) == (!!TLS.PartialBundle)));
		return Result;
	}

	/**
	 * Puts a memory block previously obtained from Allocate() back on the free list for future use.
	 *
	 * @param Item The item to free.
	 * @see Allocate
	 */
	void Push(T *Item)
	{
		NumUsed.Decrement();
		NumFree.Increment();
		FThreadLocalCache& TLS = GetTLS();
		if (TLS.NumPartial >= NUM_PER_BUNDLE)
		{
			if (TLS.FullBundle)
			{
				GlobalFreeListBundles.Push(TLS.FullBundle);
				//TLS.FullBundle = nullptr;
			}
			TLS.FullBundle = TLS.PartialBundle;
			TLS.PartialBundle = nullptr;
			TLS.NumPartial = 0;
		}
		Item->LockFreePointerQueueNext = TLS.PartialBundle;
		TLS.PartialBundle = Item;
		TLS.NumPartial++;
	}

	/**
	 * Gets the number of allocated memory blocks that are currently in use.
	 *
	 * @return Number of used memory blocks.
	 * @see GetNumFree
	 */
	const TTrackingCounter& GetNumUsed() const
	{
		return NumUsed;
	}

	/**
	 * Gets the number of allocated memory blocks that are currently unused.
	 *
	 * @return Number of unused memory blocks.
	 * @see GetNumUsed
	 */
	const TTrackingCounter& GetNumFree() const
	{
		return NumFree;
	}

private:

	/** struct for the TLS cache. */
	struct FThreadLocalCache
	{
		T* FullBundle;
		T* PartialBundle;
		int32 NumPartial;

		FThreadLocalCache()
			: FullBundle(nullptr)
			, PartialBundle(nullptr)
			, NumPartial(0)
		{
		}
	};

	FThreadLocalCache& GetTLS()
	{
		checkSlow(FPlatformTLS::IsValidTlsSlot(TlsSlot));
		FThreadLocalCache* TLS = (FThreadLocalCache*)FPlatformTLS::GetTlsValue(TlsSlot);
		if (!TLS)
		{
			TLS = new FThreadLocalCache();
			FPlatformTLS::SetTlsValue(TlsSlot, TLS);
		}
		return *TLS;
	}

	/** Slot for TLS struct. */
	uint32 TlsSlot;

	/** Lock free list of free memory blocks, these are all linked into a bundle of NUM_PER_BUNDLE. */
	TBundleRecycler GlobalFreeListBundles;

	/** Total number of blocks outstanding and not in the free list. */
	TTrackingCounter NumUsed; 

	/** Total number of blocks in the free list. */
	TTrackingCounter NumFree;
};

/**
 * Thread safe, lock free pooling allocator of fixed size blocks that
 * never returns free space, even at shutdown
 * alignment isn't handled, assumes FMemory::Malloc will work
 */

#define USE_NIEVE_TLockFreeFixedSizeAllocator_TLSCacheBase (0) // this is useful for find who really leaked
template<int32 SIZE, typename TBundleRecycler, typename TTrackingCounter = FNoopCounter>
class TLockFreeFixedSizeAllocator_TLSCacheBase : public FNoncopyable
{
	enum
	{
		NUM_PER_BUNDLE=32,
	};
public:

	TLockFreeFixedSizeAllocator_TLSCacheBase()
	{
		static_assert(SIZE >= sizeof(void*) && SIZE % sizeof(void*) == 0, "Blocks in TLockFreeFixedSizeAllocator must be at least the size of a pointer.");
		check(IsInGameThread());
		TlsSlot = FPlatformTLS::AllocTlsSlot();
		check(FPlatformTLS::IsValidTlsSlot(TlsSlot));
	}
	/** Destructor, leaks all of the memory **/
	~TLockFreeFixedSizeAllocator_TLSCacheBase()
	{
		FPlatformTLS::FreeTlsSlot(TlsSlot);
		TlsSlot = 0;
	}

	/**
	 * Allocates a memory block of size SIZE.
	 *
	 * @return Pointer to the allocated memory.
	 * @see Free
	 */
	FORCEINLINE void* Allocate()
	{
#if USE_NIEVE_TLockFreeFixedSizeAllocator_TLSCacheBase
		return FMemory::Malloc(SIZE);
#else
		FThreadLocalCache& TLS = GetTLS();

		if (!TLS.PartialBundle)
		{
			if (TLS.FullBundle)
			{
				TLS.PartialBundle = TLS.FullBundle;
				TLS.FullBundle = nullptr;
			}
			else
			{
				TLS.PartialBundle = GlobalFreeListBundles.Pop();
				if (!TLS.PartialBundle)
				{
					TLS.PartialBundle = (void**)FMemory::Malloc(SIZE * NUM_PER_BUNDLE);
					void **Next = TLS.PartialBundle;
					for (int32 Index = 0; Index < NUM_PER_BUNDLE - 1; Index++)
					{
						void* NextNext = (void*)(((uint8*)Next) + SIZE);
						*Next = NextNext;
						Next = (void**)NextNext;
					}
					*Next = nullptr;
					NumFree.Add(NUM_PER_BUNDLE);
				}
			}
			TLS.NumPartial = NUM_PER_BUNDLE;
		}
		NumUsed.Increment();
		NumFree.Decrement();
		void* Result = (void*)TLS.PartialBundle;
		TLS.PartialBundle = (void**)*TLS.PartialBundle;
		TLS.NumPartial--;
		check(TLS.NumPartial >= 0 && ((!!TLS.NumPartial) == (!!TLS.PartialBundle)));
		return Result;
#endif
	}

	/**
	 * Puts a memory block previously obtained from Allocate() back on the free list for future use.
	 *
	 * @param Item The item to free.
	 * @see Allocate
	 */
	FORCEINLINE void Free(void *Item)
	{
#if USE_NIEVE_TLockFreeFixedSizeAllocator_TLSCacheBase
		return FMemory::Free(Item);
#else
		NumUsed.Decrement();
		NumFree.Increment();
		FThreadLocalCache& TLS = GetTLS();
		if (TLS.NumPartial >= NUM_PER_BUNDLE)
		{
			if (TLS.FullBundle)
			{
				GlobalFreeListBundles.Push(TLS.FullBundle);
				//TLS.FullBundle = nullptr;
			}
			TLS.FullBundle = TLS.PartialBundle;
			TLS.PartialBundle = nullptr;
			TLS.NumPartial = 0;
		}
		*(void**)Item = (void*)TLS.PartialBundle;
		TLS.PartialBundle = (void**)Item;
		TLS.NumPartial++;
#endif
	}

	/**
	 * Gets the number of allocated memory blocks that are currently in use.
	 *
	 * @return Number of used memory blocks.
	 * @see GetNumFree
	 */
	const TTrackingCounter& GetNumUsed() const
	{
		return NumUsed;
	}

	/**
	 * Gets the number of allocated memory blocks that are currently unused.
	 *
	 * @return Number of unused memory blocks.
	 * @see GetNumUsed
	 */
	const TTrackingCounter& GetNumFree() const
	{
		return NumFree;
	}

private:

	/** struct for the TLS cache. */
	struct FThreadLocalCache
	{
		void **FullBundle;
		void **PartialBundle;
		int32 NumPartial;

		FThreadLocalCache()
			: FullBundle(nullptr)
			, PartialBundle(nullptr)
			, NumPartial(0)
		{
		}
	};

	FThreadLocalCache& GetTLS()
	{
		checkSlow(FPlatformTLS::IsValidTlsSlot(TlsSlot));
		FThreadLocalCache* TLS = (FThreadLocalCache*)FPlatformTLS::GetTlsValue(TlsSlot);
		if (!TLS)
		{
			TLS = new FThreadLocalCache();
			FPlatformTLS::SetTlsValue(TlsSlot, TLS);
		}
		return *TLS;
	}

	/** Slot for TLS struct. */
	uint32 TlsSlot;

	/** Lock free list of free memory blocks, these are all linked into a bundle of NUM_PER_BUNDLE. */
	TBundleRecycler GlobalFreeListBundles;

	/** Total number of blocks outstanding and not in the free list. */
	TTrackingCounter NumUsed; 

	/** Total number of blocks in the free list. */
	TTrackingCounter NumFree;
};

// in these cases it is critical that no thread starves because they must advance and other threads are blocked.

CORE_API void LockFreeCriticalSpin(int32& SpinCount);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

CORE_API void DoTestCriticalStall();
extern CORE_API int32 GTestCriticalStalls;

FORCEINLINE void TestCriticalStall()
{
	if (GTestCriticalStalls)
	{
		DoTestCriticalStall();
	}
}
#else
FORCEINLINE void TestCriticalStall()
{
}
#endif

template<class T, int TPaddingForCacheContention>
class FLockFreePointerQueueBaseSingleConsumerIntrusive : public FNoncopyable
{
public:
	FLockFreePointerQueueBaseSingleConsumerIntrusive()
		: Head(GetStub())
		, Tail(GetStub())
	{
		GetStub()->LockFreePointerQueueNext = nullptr;
	}

	void Push(T* Link)
	{
		Link->LockFreePointerQueueNext = nullptr;
		T* Prev = (T*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, Link);
		TestCriticalStall();
		Prev->LockFreePointerQueueNext = Link;
		//FPlatformMisc::MemoryBarrier();
	}

	T* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* LocalTail = Tail; 
			T* LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext); 
			if (LocalTail == GetStub())
			{
				if (!LocalTailNext)
				{
					if (LocalTail != Head)
					{
						LockFreeCriticalSpin(SpinCount);
						continue;
					}
					return nullptr;
				}
				Tail = LocalTailNext;
				LocalTail = LocalTailNext;
				LocalTailNext = (T*)(LocalTailNext->LockFreePointerQueueNext);
			}
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			if (LocalTail != Head)
			{
				LockFreeCriticalSpin(SpinCount);
				continue;
			}
			Push(GetStub());
			LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext); 
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}
	/**	
	 *	Check if the list is empty.
	 *
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	bool IsEmpty() const
	{
		//FPlatformMisc::MemoryBarrier();
		T* LocalTail = Tail; 
		return Head == LocalTail && GetStub() == LocalTail;
	}

private:
	uint8 PadToAvoidContention1[TPaddingForCacheContention];
	T* Head;
	uint8 PadToAvoidContention2[TPaddingForCacheContention];
	T* Tail;
	uint8 PadToAvoidContention3[TPaddingForCacheContention];

	T* GetStub() const
	{
		return (T*)&StubStorage; // we aren't going to do anything with this other than mess with internal links
	}
	TAlignedBytes<sizeof(T), ALIGNOF(void*)> StubStorage;
};

template<class T>
class FLockFreePointerQueueBaseSingleConsumerIntrusive<T, 0> : public FNoncopyable
{
public:
	FLockFreePointerQueueBaseSingleConsumerIntrusive()
		: Head(GetStub())
		, Tail(GetStub())
	{
		GetStub()->LockFreePointerQueueNext = nullptr;
	}

	void Push(T* Link)
	{
		Link->LockFreePointerQueueNext = nullptr;
		T* Prev = (T*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, Link);
		TestCriticalStall();
		Prev->LockFreePointerQueueNext = Link;
		//FPlatformMisc::MemoryBarrier();
	}

	T* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* LocalTail = Tail;
			T* LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext);
			if (LocalTail == GetStub())
			{
				if (!LocalTailNext)
				{
					if (LocalTail != Head)
					{
						LockFreeCriticalSpin(SpinCount);
						continue;
					}
					return nullptr;
				}
				Tail = LocalTailNext;
				LocalTail = LocalTailNext;
				LocalTailNext = (T*)(LocalTailNext->LockFreePointerQueueNext);
			}
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			if (LocalTail != Head)
			{
				LockFreeCriticalSpin(SpinCount);
				continue;
			}
			Push(GetStub());
			LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext);
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}
	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	bool IsEmpty() const
	{
		//FPlatformMisc::MemoryBarrier();
		T* LocalTail = Tail;
		return Head == LocalTail && GetStub() == LocalTail;
	}

private:
	T* Head;
	T* Tail;

	T* GetStub() const
	{
		return (T*)&StubStorage; // we aren't going to do anything with this other than mess with internal links
	}
	TAlignedBytes<sizeof(T), ALIGNOF(void*)> StubStorage;
};

struct FLockFreeLink
{
	FLockFreeLink* Next;
	void *Payload;
};

template<int TPaddingForCacheContention>
class FSpinLocked_LIFO
{
public:
	void** Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			if (!Lock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Lock, 1, 0) == 0)
			{
				TestCriticalStall();
				void** Result = nullptr;
				if (Queue.Num())
				{
					Result = Queue.Pop(false);
				}
				FPlatformMisc::MemoryBarrier();
				Lock = 0;
				return Result;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}
	void Push(void** Item)
	{
		int32 SpinCount = 0;
		while (true)
		{
			if (!Lock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Lock, 1, 0) == 0)
			{
				TestCriticalStall();
				Queue.Add(Item);
				FPlatformMisc::MemoryBarrier();
				Lock = 0;
				return;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}
private:
	TArray<void **> Queue;
	uint8 PadToAvoidContention1[TPaddingForCacheContention];
	int32 Lock;
	uint8 PadToAvoidContention2[TPaddingForCacheContention];
};

template<>
class FSpinLocked_LIFO<0>
{
public:
	void** Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			if (!Lock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Lock, 1, 0) == 0)
			{
				TestCriticalStall();
				void** Result = nullptr;
				if (Queue.Num())
				{
					Result = Queue.Pop(false);
				}
				FPlatformMisc::MemoryBarrier();
				Lock = 0;
				return Result;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}
	void Push(void** Item)
	{
		int32 SpinCount = 0;
		while (true)
		{
			if (!Lock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Lock, 1, 0) == 0)
			{
				TestCriticalStall();
				Queue.Add(Item);
				FPlatformMisc::MemoryBarrier();
				Lock = 0;
				return;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}
private:
	TArray<void **> Queue;
	int32 Lock;
};

class FLockFreePointerQueueBaseLinkAllocator : public FNoncopyable
{
public:
	static CORE_API FLockFreePointerQueueBaseLinkAllocator& Get();

	FORCEINLINE void Free(FLockFreeLink* Link)
	{
		Link->~FLockFreeLink();
		InnerAllocator.Free(Link);
	}

	FORCEINLINE FLockFreeLink* Alloc()
	{
		return new (InnerAllocator.Allocate()) FLockFreeLink();
	}

	/**
	* Gets the number of allocated memory blocks that are currently in use.
	*
	* @return Number of used memory blocks.
	*/
	const int32 GetNumUsed() const
	{
		return InnerAllocator.GetNumUsed().GetValue();
	}

	/**
	* Gets the number of allocated memory blocks that are currently unused.
	*
	* @return Number of allocated, but unused memory blocks.
	*/
	const int32 GetNumFree() const
	{
		return InnerAllocator.GetNumFree().GetValue();
	}

private:
	TLockFreeFixedSizeAllocator_TLSCacheBase<sizeof(FLockFreeLink), FSpinLocked_LIFO<PLATFORM_CACHE_LINE_SIZE>,
#if 1 || UE_BUILD_SHIPPING || UE_BUILD_TEST
		FNoopCounter
#else
		FThreadSafeCounter
#endif
	> InnerAllocator;
};

template<int TPaddingForCacheContention>
class FLockFreePointerQueueBaseSingleConsumer : public FNoncopyable
{
public:
	FLockFreePointerQueueBaseSingleConsumer()
		: Head(FLockFreePointerQueueBaseLinkAllocator::Get().Alloc())
		, Tail(Head)
	{
	}
	~FLockFreePointerQueueBaseSingleConsumer()
	{
		check(IsEmpty());
		FLockFreePointerQueueBaseLinkAllocator::Get().Free(Head);
	}

	void Push(FLockFreeLink* Link)
	{
		Link->Next = nullptr;
		FLockFreeLink* Prev = (FLockFreeLink*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, Link);
		TestCriticalStall();
		Prev->Next = Link;
		//FPlatformMisc::MemoryBarrier();
	}

	FLockFreeLink* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			FLockFreeLink* LocalTail = Tail; 
			FLockFreeLink* LocalTailNext = LocalTail->Next; 
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				LocalTail->Payload = LocalTailNext->Payload;
				return LocalTail;
			}
			else if (Head == LocalTail)
			{
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}

	/**	
	 *	Check if the list is empty.
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	bool IsEmpty() const
	{
		//FPlatformMisc::MemoryBarrier();
		return Head == Tail;
	}

private:

	uint8 PadToAvoidContention1[TPaddingForCacheContention];
	FLockFreeLink* Head;
	uint8 PadToAvoidContention2[TPaddingForCacheContention];
	FLockFreeLink* Tail;
	uint8 PadToAvoidContention3[TPaddingForCacheContention];
};

template<>
class FLockFreePointerQueueBaseSingleConsumer<0> : public FNoncopyable
{
public:
	FLockFreePointerQueueBaseSingleConsumer()
		: Head(FLockFreePointerQueueBaseLinkAllocator::Get().Alloc())
		, Tail(Head)
	{
	}

	~FLockFreePointerQueueBaseSingleConsumer()
	{
		check(IsEmpty());
		FLockFreePointerQueueBaseLinkAllocator::Get().Free(Head);
	}

	void Push(FLockFreeLink* Link)
	{
		Link->Next = nullptr;
		FLockFreeLink* Prev = (FLockFreeLink*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, Link);
		TestCriticalStall();
		Prev->Next = Link;
		//FPlatformMisc::MemoryBarrier();
	}

	FLockFreeLink* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			FLockFreeLink* LocalTail = Tail;
			FLockFreeLink* LocalTailNext = LocalTail->Next;
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				LocalTail->Payload = LocalTailNext->Payload;
				return LocalTail;
			}
			else if (Head == LocalTail)
			{
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}

	/**
	*	Check if the list is empty.
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	bool IsEmpty() const
	{
		//FPlatformMisc::MemoryBarrier();
		return Head == Tail;
	}

private:

	FLockFreeLink* Head;
	FLockFreeLink* Tail;
};

template<int TPaddingForCacheContention>
class FCloseableLockFreePointerQueueBaseSingleConsumer : public FNoncopyable
{
public:
	FCloseableLockFreePointerQueueBaseSingleConsumer()
		: Head(FLockFreePointerQueueBaseLinkAllocator::Get().Alloc())
		, Tail(Head)
	{

	}

	~FCloseableLockFreePointerQueueBaseSingleConsumer()
	{
		check(IsEmpty());
		FLockFreePointerQueueBaseLinkAllocator::Get().Free(Head);
	}

	/**	
	 *	Push an item onto the head of the list, unless the list is closed
	 *	@param NewItem, the new item to push on the list, cannot be nullptr
	 *	@return true if the item was pushed on the list, false if the list was closed.
	 */
	bool PushIfNotClosed(FLockFreeLink* Link)
	{
		checkLockFreePointerList(Link);
		Link->Next = nullptr;
		while (true)
		{
			FLockFreeLink* LocalHead = Head; 
			if (!IsClosed(LocalHead))
			{
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
				{
					TestCriticalStall();
					LocalHead->Next = Link;
					return true;
				}
			}
			else
			{
				break;
			}
		}
		return false;
	}

	/**	
	 *	Push an item onto the head of the list, opening it first if necessary
	 *	@param NewItem, the new item to push on the list, cannot be nullptr
	 *	@return true if the list needed to be opened first, false if the list was not closed before our push
	 */
	bool ReopenIfClosedAndPush(FLockFreeLink *Link)
	{
		bool bWasReopenedByMe = false;
		Link->Next = nullptr;
		while (true)
		{
			FLockFreeLink* LocalHead = Head; 
			if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
			{
				TestCriticalStall();
				FLockFreeLink* LocalHeadOpen = ClearClosed(LocalHead); 
				bWasReopenedByMe = IsClosed(LocalHead);
				LocalHeadOpen->Next = Link;
				break;
			}
		}
		return bWasReopenedByMe;
	}

	/**	
	 *	Close the list if it is empty
	 *	@return true if this call actively closed the list
	 */
	bool CloseIfEmpty()
	{
		int32 SpinCount = 0;
		while (true)
		{
			FPlatformMisc::MemoryBarrier();
			FLockFreeLink* LocalTail = Tail; 
			FLockFreeLink* LocalHead = Head; 
			checkLockFreePointerList(!IsClosed(LocalHead));
			if (LocalTail == LocalHead)
			{
				FLockFreeLink* LocalHeadClosed = SetClosed(LocalHead); 
				checkLockFreePointerList(LocalHeadClosed != LocalHead);
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, LocalHeadClosed, LocalHead) == LocalHead)
				{
					return true;
				}
			}
			else
			{
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return false;
	}
	/**	
	 *	Pop an item from the list or return nullptr if the list is empty
	 *	@return The popped item, if any
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	FLockFreeLink* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			FLockFreeLink* LocalTail = Tail; 
			checkLockFreePointerList(!IsClosed(LocalTail));
			FLockFreeLink* LocalTailNext = LocalTail->Next; 
			checkLockFreePointerList(!IsClosed(LocalTailNext));
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				LocalTail->Payload = LocalTailNext->Payload;
				return LocalTail;
			}
			else if (Head == LocalTail)
			{
				break;
			}
			checkLockFreePointerList(!IsClosed(Head));
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}
	/**	
	 *	Check if the list is empty.
	 *
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	bool IsEmpty() const
	{
		FPlatformMisc::MemoryBarrier();
		return ClearClosed(Head) == Tail;
	}

	/**	
	 *	Check if the list is closed
	 *	@return true if the list is closed.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could open or close the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is only closed once it is lifetime, so a return of true here means it is closed forever.
	 */
	FORCEINLINE bool IsClosed() const  // caution, if this returns false, that does not mean the list is open!
	{
		FPlatformMisc::MemoryBarrier();
		return IsClosed(Head);
	}
	/**	
	 *	Not thread safe, used to reset the list for recycling without freeing the stub
	 *	@return true if the list is closed.
	 */
	void Reset()
	{
		Head = ClearClosed(Head);
		FPlatformMisc::MemoryBarrier();
		checkLockFreePointerList(Head == Tail); // we don't clear the list here, we assume it is already clear and possibly closed
	}

private:
	// we will use the lowest bit for the closed state
	FORCEINLINE static bool IsClosed(FLockFreeLink* HeadToTest)
	{
		return !!(UPTRINT(HeadToTest) & 1);
	}
	FORCEINLINE static FLockFreeLink* SetClosed(FLockFreeLink* In)
	{
		return (FLockFreeLink*)(UPTRINT(In) | 1);
	}
	FORCEINLINE static FLockFreeLink* ClearClosed(FLockFreeLink* In)
	{
		return (FLockFreeLink*)(UPTRINT(In) & ~UPTRINT(1));
	}

	uint8 PadToAvoidContention1[TPaddingForCacheContention];
	FLockFreeLink* Head;
	uint8 PadToAvoidContention2[TPaddingForCacheContention];
	FLockFreeLink* Tail;
	uint8 PadToAvoidContention3[TPaddingForCacheContention];
};

template<>
class FCloseableLockFreePointerQueueBaseSingleConsumer<0> : public FNoncopyable
{
public:
	FCloseableLockFreePointerQueueBaseSingleConsumer()
		: Head(FLockFreePointerQueueBaseLinkAllocator::Get().Alloc())
		, Tail(Head)
	{
	}
	~FCloseableLockFreePointerQueueBaseSingleConsumer()
	{
		check(IsEmpty());
		FLockFreePointerQueueBaseLinkAllocator::Get().Free(Head);
	}

	/**
	*	Push an item onto the head of the list, unless the list is closed
	*	@param NewItem, the new item to push on the list, cannot be nullptr
	*	@return true if the item was pushed on the list, false if the list was closed.
	*/
	bool PushIfNotClosed(FLockFreeLink* Link)
	{
		checkLockFreePointerList(Link);
		Link->Next = nullptr;
		while (true)
		{
			FLockFreeLink* LocalHead = Head;
			if (!IsClosed(LocalHead))
			{
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
				{
					TestCriticalStall();
					LocalHead->Next = Link;
					return true;
				}
			}
			else
			{
				break;
			}
		}
		return false;
	}

	/**
	*	Push an item onto the head of the list, opening it first if necessary
	*	@param NewItem, the new item to push on the list, cannot be nullptr
	*	@return true if the list needed to be opened first, false if the list was not closed before our push
	*/
	bool ReopenIfClosedAndPush(FLockFreeLink *Link)
	{
		bool bWasReopenedByMe = false;
		Link->Next = nullptr;
		while (true)
		{
			FLockFreeLink* LocalHead = Head;
			if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
			{
				TestCriticalStall();
				FLockFreeLink* LocalHeadOpen = ClearClosed(LocalHead);
				bWasReopenedByMe = IsClosed(LocalHead);
				LocalHeadOpen->Next = Link;
				break;
			}
		}
		return bWasReopenedByMe;
	}

	/**
	*	Close the list if it is empty
	*	@return true if this call actively closed the list
	*/
	bool CloseIfEmpty()
	{
		int32 SpinCount = 0;
		while (true)
		{
			FPlatformMisc::MemoryBarrier();
			FLockFreeLink* LocalTail = Tail;
			FLockFreeLink* LocalHead = Head;
			checkLockFreePointerList(!IsClosed(LocalHead));
			if (LocalTail == LocalHead)
			{
				FLockFreeLink* LocalHeadClosed = SetClosed(LocalHead);
				checkLockFreePointerList(LocalHeadClosed != LocalHead);
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, LocalHeadClosed, LocalHead) == LocalHead)
				{
					return true;
				}
			}
			else
			{
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return false;
	}
	/**
	*	Pop an item from the list or return nullptr if the list is empty
	*	@return The popped item, if any
	*	CAUTION: This method should not be used unless the list is known to not be closed.
	*/
	FLockFreeLink* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			FLockFreeLink* LocalTail = Tail;
			checkLockFreePointerList(!IsClosed(LocalTail));
			FLockFreeLink* LocalTailNext = LocalTail->Next;
			checkLockFreePointerList(!IsClosed(LocalTailNext));
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				LocalTail->Payload = LocalTailNext->Payload;
				return LocalTail;
			}
			else if (Head == LocalTail)
			{
				break;
			}
			checkLockFreePointerList(!IsClosed(Head));
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}
	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	bool IsEmpty() const
	{
		FPlatformMisc::MemoryBarrier();
		return ClearClosed(Head) == Tail;
	}

	/**
	*	Check if the list is closed
	*	@return true if the list is closed.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could open or close the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is only closed once it is lifetime, so a return of true here means it is closed forever.
	*/
	FORCEINLINE bool IsClosed() const  // caution, if this returns false, that does not mean the list is open!
	{
		FPlatformMisc::MemoryBarrier();
		return IsClosed(Head);
	}
	/**
	*	Not thread safe, used to reset the list for recycling without freeing the stub
	*	@return true if the list is closed.
	*/
	void Reset()
	{
		Head = ClearClosed(Head);
		FPlatformMisc::MemoryBarrier();
		checkLockFreePointerList(Head == Tail); // we don't clear the list here, we assume it is already clear and possibly closed
	}

private:
	// we will use the lowest bit for the closed state
	FORCEINLINE static bool IsClosed(FLockFreeLink* HeadToTest)
	{
		return !!(UPTRINT(HeadToTest) & 1);
	}
	FORCEINLINE static FLockFreeLink* SetClosed(FLockFreeLink* In)
	{
		return (FLockFreeLink*)(UPTRINT(In) | 1);
	}
	FORCEINLINE static FLockFreeLink* ClearClosed(FLockFreeLink* In)
	{
		return (FLockFreeLink*)(UPTRINT(In) & ~UPTRINT(1));
	}

	FLockFreeLink* Head;
	FLockFreeLink* Tail;
};

template<class T, int TPaddingForCacheContention>
class FCloseableLockFreePointerQueueBaseSingleBaseConsumerIntrusive : public FNoncopyable
{
public:
	FCloseableLockFreePointerQueueBaseSingleBaseConsumerIntrusive()
		: Head(GetStub())
		, Tail(GetStub())
	{
		GetStub()->LockFreePointerQueueNext = nullptr;
	}
#if 0
	/**	
	 *	Push an item onto the head of the list, unless the list is closed
	 *	@param NewItem, the new item to push on the list, cannot be nullptr
	 *	@return true if the item was pushed on the list, false if the list was closed.
	 */
	bool PushIfNotClosed(T* Link)
	{
		checkLockFreePointerList(Link);
		Link->LockFreePointerQueueNext = nullptr;
		while (true)
		{
			T* LocalHead = Head; 
			if (!IsClosed(LocalHead))
			{
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
				{
					TestCriticalStall();
					LocalHead->LockFreePointerQueueNext = Link;
					return true;
				}
			}
			else
			{
				break;
			}
		}
		return false;
	}
#endif
	/**	
	 *	Push an item onto the head of the list, opening it first if necessary
	 *	@param NewItem, the new item to push on the list, cannot be nullptr
	 *	@return true if the list needed to be opened first, false if the list was not closed before our push
	 */
	bool ReopenIfClosedAndPush(T *Link)
	{
		bool bWasReopenedByMe = false;
		checkLockFreePointerList(Link);
		Link->LockFreePointerQueueNext = nullptr;
		while (true)
		{
			T* LocalHead = Head; 
			if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
			{
				TestCriticalStall();
				T* LocalHeadOpen = ClearClosed(LocalHead); 
				bWasReopenedByMe = IsClosed(LocalHead);
				LocalHeadOpen->LockFreePointerQueueNext = Link;
				break;
			}
		}
		return bWasReopenedByMe;
	}

	/**	
	 *	Close the list if it is empty
	 *	@return true if this call actively closed the list
	 */
	bool CloseIfEmpty()
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* LocalTail = Tail; 
			T* LocalHead = Head; 
			checkLockFreePointerList(!IsClosed(LocalHead));
			if (LocalTail == LocalHead)
			{
				T* LocalHeadClosed = SetClosed(LocalHead); 
				checkLockFreePointerList(LocalHeadClosed != LocalHead);
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, LocalHeadClosed, LocalHead) == LocalHead)
				{
					return true;
				}
			}
			else
			{
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return false;
	}

	/**	
	 *	Pop an item from the list or return nullptr if the list is empty
	 *	@return The popped item, if any
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	FORCEINLINE T* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* LocalTail = Tail; 
			checkLockFreePointerList(!IsClosed(LocalTail));
			T* LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext); 
			checkLockFreePointerList(!IsClosed(LocalTailNext));
			if (LocalTail == GetStub())
			{
				if (!LocalTailNext)
				{
					if (LocalTail != Head)
					{
						LockFreeCriticalSpin(SpinCount);
						continue;
					}
					return nullptr;
				}
				Tail = LocalTailNext;
				LocalTail = LocalTailNext;
				LocalTailNext = (T*)(LocalTailNext->LockFreePointerQueueNext);
			}
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			if (LocalTail != Head)
			{
				LockFreeCriticalSpin(SpinCount);
				continue;
			}
			Push(GetStub());
			LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext); 
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}
	/**	
	 *	Check if the list is closed
	 *	@return true if the list is closed.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could open or close the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is only closed once it is lifetime, so a return of true here means it is closed forever.
	 */
	FORCEINLINE bool IsClosed() const  // caution, if this returns false, that does not mean the list is open!
	{
		FPlatformMisc::MemoryBarrier();
		return IsClosed(Head);
	}
#if 0
	/**	
	 *	Check if the list is empty.
	 *
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	bool IsEmpty() const
	{
		FPlatformMisc::MemoryBarrier();
		return ClearClosed(Head) == Tail;
	}

	/**	
	 *	Not thread safe, used to reset the list for recycling without freeing the stub
	 *	@return true if the list is closed.
	 */
	void Reset()
	{
		Head = ClearClosed(Head);
		FPlatformMisc::MemoryBarrier();
		checkLockFreePointerList(Head == Tail); // we don't clear the list here, we assume it is already clear and possibly closed
	}
#endif
private:
	void Push(T* Link)
	{
		Link->LockFreePointerQueueNext = nullptr;
		T* Prev = (T*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, Link);
		TestCriticalStall();
		Prev->LockFreePointerQueueNext = Link;
		//FPlatformMisc::MemoryBarrier();
	}

	// we will use the lowest bit for the closed state
	FORCEINLINE static bool IsClosed(T* HeadToTest)
	{
		return !!(UPTRINT(HeadToTest) & 1);
	}
	FORCEINLINE static T* SetClosed(T* In)
	{
		return (T*)(UPTRINT(In) | 1);
	}
	FORCEINLINE static T* ClearClosed(T* In)
	{
		return (T*)(UPTRINT(In) & ~UPTRINT(1));
	}


	uint8 PadToAvoidContention1[TPaddingForCacheContention];
	T* Head;
	uint8 PadToAvoidContention2[TPaddingForCacheContention];
	T* Tail;
	uint8 PadToAvoidContention3[TPaddingForCacheContention];

	T* GetStub() const
	{
		return (T*)&StubStorage; // we aren't going to do anything with this other than mess with internal links
	}
	TAlignedBytes<sizeof(T), ALIGNOF(void*)> StubStorage;
};

template<class T>
class FCloseableLockFreePointerQueueBaseSingleBaseConsumerIntrusive<T, 0> : public FNoncopyable
{
public:
	FCloseableLockFreePointerQueueBaseSingleBaseConsumerIntrusive()
		: Head(GetStub())
		, Tail(GetStub())
	{
		GetStub()->LockFreePointerQueueNext = nullptr;
	}
#if 0
	/**
	*	Push an item onto the head of the list, unless the list is closed
	*	@param NewItem, the new item to push on the list, cannot be nullptr
	*	@return true if the item was pushed on the list, false if the list was closed.
	*/
	bool PushIfNotClosed(T* Link)
	{
		checkLockFreePointerList(Link);
		Link->LockFreePointerQueueNext = nullptr;
		while (true)
		{
			T* LocalHead = Head;
			if (!IsClosed(LocalHead))
			{
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
				{
					TestCriticalStall();
					LocalHead->LockFreePointerQueueNext = Link;
					return true;
				}
			}
			else
			{
				break;
			}
		}
		return false;
	}
#endif
	/**
	*	Push an item onto the head of the list, opening it first if necessary
	*	@param NewItem, the new item to push on the list, cannot be nullptr
	*	@return true if the list needed to be opened first, false if the list was not closed before our push
	*/
	bool ReopenIfClosedAndPush(T *Link)
	{
		bool bWasReopenedByMe = false;
		checkLockFreePointerList(Link);
		Link->LockFreePointerQueueNext = nullptr;
		while (true)
		{
			T* LocalHead = Head;
			if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, Link, LocalHead) == LocalHead)
			{
				TestCriticalStall();
				T* LocalHeadOpen = ClearClosed(LocalHead);
				bWasReopenedByMe = IsClosed(LocalHead);
				LocalHeadOpen->LockFreePointerQueueNext = Link;
				break;
			}
		}
		return bWasReopenedByMe;
	}

	/**
	*	Close the list if it is empty
	*	@return true if this call actively closed the list
	*/
	bool CloseIfEmpty()
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* LocalTail = Tail;
			T* LocalHead = Head;
			checkLockFreePointerList(!IsClosed(LocalHead));
			if (LocalTail == LocalHead)
			{
				T* LocalHeadClosed = SetClosed(LocalHead);
				checkLockFreePointerList(LocalHeadClosed != LocalHead);
				if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, LocalHeadClosed, LocalHead) == LocalHead)
				{
					return true;
				}
			}
			else
			{
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return false;
	}

	/**
	*	Pop an item from the list or return nullptr if the list is empty
	*	@return The popped item, if any
	*	CAUTION: This method should not be used unless the list is known to not be closed.
	*/
	FORCEINLINE T* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* LocalTail = Tail;
			checkLockFreePointerList(!IsClosed(LocalTail));
			T* LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext);
			checkLockFreePointerList(!IsClosed(LocalTailNext));
			if (LocalTail == GetStub())
			{
				if (!LocalTailNext)
				{
					if (LocalTail != Head)
					{
						LockFreeCriticalSpin(SpinCount);
						continue;
					}
					return nullptr;
				}
				Tail = LocalTailNext;
				LocalTail = LocalTailNext;
				LocalTailNext = (T*)(LocalTailNext->LockFreePointerQueueNext);
			}
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			if (LocalTail != Head)
			{
				LockFreeCriticalSpin(SpinCount);
				continue;
			}
			Push(GetStub());
			LocalTailNext = (T*)(LocalTail->LockFreePointerQueueNext);
			if (LocalTailNext)
			{
				Tail = LocalTailNext;
				return LocalTail;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return nullptr;
	}
	/**
	*	Check if the list is closed
	*	@return true if the list is closed.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could open or close the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is only closed once it is lifetime, so a return of true here means it is closed forever.
	*/
	FORCEINLINE bool IsClosed() const  // caution, if this returns false, that does not mean the list is open!
	{
		FPlatformMisc::MemoryBarrier();
		return IsClosed(Head);
	}
#if 0
	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	bool IsEmpty() const
	{
		FPlatformMisc::MemoryBarrier();
		return ClearClosed(Head) == Tail;
	}

	/**
	*	Not thread safe, used to reset the list for recycling without freeing the stub
	*	@return true if the list is closed.
	*/
	void Reset()
	{
		Head = ClearClosed(Head);
		FPlatformMisc::MemoryBarrier();
		checkLockFreePointerList(Head == Tail); // we don't clear the list here, we assume it is already clear and possibly closed
	}
#endif
private:
	void Push(T* Link)
	{
		Link->LockFreePointerQueueNext = nullptr;
		T* Prev = (T*)FPlatformAtomics::InterlockedExchangePtr((void**)&Head, Link);
		TestCriticalStall();
		Prev->LockFreePointerQueueNext = Link;
		//FPlatformMisc::MemoryBarrier();
	}

	// we will use the lowest bit for the closed state
	FORCEINLINE static bool IsClosed(T* HeadToTest)
	{
		return !!(UPTRINT(HeadToTest) & 1);
	}
	FORCEINLINE static T* SetClosed(T* In)
	{
		return (T*)(UPTRINT(In) | 1);
	}
	FORCEINLINE static T* ClearClosed(T* In)
	{
		return (T*)(UPTRINT(In) & ~UPTRINT(1));
	}

	T* Head;
	T* Tail;

	T* GetStub() const
	{
		return (T*)&StubStorage; // we aren't going to do anything with this other than mess with internal links
	}
	TAlignedBytes<sizeof(T), ALIGNOF(void*)> StubStorage;
};

class FDequeueCache : public FNoncopyable
{
	enum
	{
		DequeueCacheSize = 256, // probably needs to be a power of two so wraparound works right
	};

	typedef uint32 TCounter; // must be unsigned
	typedef int32 TSignedCounter; // must be signed

public:
	FDequeueCache()
		: NumPopped(0)
		, NumPushed(0)
	{
		FMemory::Memzero(&Available[0], sizeof(void*) * DequeueCacheSize);
	}

	FORCEINLINE void* Pop()
	{
		int32 SpinCount = 0;
		while (true)
		{
			TCounter LocalNumPushed = NumPushed;
			TCounter LocalNumPopped = NumPopped;
			if (TSignedCounter(LocalNumPushed - LocalNumPopped) < 0) 
			{
				LockFreeCriticalSpin(SpinCount);
				continue;
			}
			if (LocalNumPushed == LocalNumPopped)
			{
				break;
			}
			uint32 Slot = LocalNumPopped % DequeueCacheSize;
			FPlatformMisc::MemoryBarrier();
			void *ReturnPtr = Available[Slot];
			TCounter Result = (TCounter)FPlatformAtomics::InterlockedCompareExchange((volatile TSignedCounter*)&NumPopped, LocalNumPopped + 1, LocalNumPopped);
			if (Result == LocalNumPopped)
			{
				check(ReturnPtr);
				return ReturnPtr;
			}
		}
		return nullptr;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return NumPushed == NumPopped;
	}

	FORCEINLINE bool IsFull() const
	{
		return NumPushed - NumPopped >= TCounter(DequeueCacheSize);
	}

	// must not be called concurrently!
	FORCEINLINE bool Push(void* Item)
	{
		TCounter LocalNumPushed = NumPushed;
		TCounter LocalNumPopped = NumPopped;
		if (LocalNumPushed - LocalNumPopped >= TCounter(DequeueCacheSize))
		{
			return false;
		}
		check(TSignedCounter(LocalNumPushed - LocalNumPopped) >= 0); // wraparound is ok, this would mean we have at least 16GB of queue for 32 bits...which is not possible
		Available[LocalNumPushed % DequeueCacheSize] = Item;
		FPlatformMisc::MemoryBarrier();
		verify(++NumPushed == LocalNumPushed + 1);
		return true;
	}

private:

	TCounter NumPopped;
	void *Available[DequeueCacheSize];
	TCounter NumPushed;
};



template<class T, int TPaddingForCacheContention>
class FLockFreePointerListFIFOIntrusive : public FNoncopyable
{
public:

	FLockFreePointerListFIFOIntrusive()
		: DequeueLock(0)
	{
	}

	void Push(T *Link)
	{
		IncomingQueue.Push(Link);
	}

	FORCEINLINE T* Pop(bool bOkToSpeculativelyReturnNull = false)
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* Result = (T*)DequeueCache.Pop();
			if (Result)
			{
				return Result;
			}
			if (bOkToSpeculativelyReturnNull && IncomingQueue.IsEmpty())
			{
				return nullptr;
			}

			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				Result = (T*)DequeueCache.Pop();
				if (Result)
				{
					// someone beat us to it
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return Result;
				}
				T* Link = IncomingQueue.Pop();
				if (!Link)
				{
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return nullptr;
				}
				check(!DequeueCache.IsFull()); // can't be full because it was empty and we have the lock
				verify(DequeueCache.Push(Link));
				while (!DequeueCache.IsFull())
				{
					T* Repush = IncomingQueue.Pop(); 
					if (!Repush)
					{
						break;
					}
					verify(DequeueCache.Push(Repush));
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}

	/**	
	 *	Check if the list is empty.
	 *
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	bool IsEmpty()  
	{
		int32 SpinCount = 0;
		bool bResult = true;
		while (true)
		{
			if (!DequeueCache.IsEmpty())
			{
				bResult = false;
				break;
			}
			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				bool bAny = false;
				if (!DequeueCache.IsFull())
				{
					T* Repush = IncomingQueue.Pop(); 
					if (!Repush)
					{
						break;
					}
					bResult = false;
					verify(DequeueCache.Push(Repush));
				}
				else
				{
					bResult = false; // we didn't pop one because the dequeue cache is full...guess it isn't empty
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return bResult;
	}
	/**	
	 *	Check if the list is empty. This is a faster, less rigorous test that can miss a queue full of stuff. Used to optimize thread restarts.
	 */
	bool IsEmptyFast()  
	{
		return DequeueCache.IsEmpty() && IncomingQueue.IsEmpty();
	}

	/**	
	 *	Pop all items from the list.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	void PopAll(TArray<T *>& Output)
	{
		while (true)
		{
			T* Item = Pop();
			if (!Item)
			{
				break;
			}
			Output.Add(Item);
		}
	}	

private:
	FLockFreePointerQueueBaseSingleConsumerIntrusive<T, TPaddingForCacheContention> IncomingQueue;
	FDequeueCache DequeueCache;
	int32 DequeueLock;
};

template<class T>
class FLockFreePointerListFIFOIntrusive<T, 0> : public FNoncopyable
{
public:

	FLockFreePointerListFIFOIntrusive()
		: DequeueLock(0)
	{
	}

	void Push(T *Link)
	{
		IncomingQueue.Push(Link);
	}

	FORCEINLINE T* Pop(bool bOkToSpeculativelyReturnNull = false)
	{
		int32 SpinCount = 0;
		while (true)
		{
			T* Result = (T*)DequeueCache.Pop();
			if (Result)
			{
				return Result;
			}
			if (bOkToSpeculativelyReturnNull && IncomingQueue.IsEmpty())
			{
				return nullptr;
			}

			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				Result = (T*)DequeueCache.Pop();
				if (Result)
				{
					// someone beat us to it
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return Result;
				}
				T* Link = IncomingQueue.Pop();
				if (!Link)
				{
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return nullptr;
				}
				check(!DequeueCache.IsFull()); // can't be full because it was empty and we have the lock
				verify(DequeueCache.Push(Link));
				while (!DequeueCache.IsFull())
				{
					T* Repush = IncomingQueue.Pop();
					if (!Repush)
					{
						break;
					}
					verify(DequeueCache.Push(Repush));
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}

	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	bool IsEmpty()
	{
		int32 SpinCount = 0;
		bool bResult = true;
		while (true)
		{
			if (!DequeueCache.IsEmpty())
			{
				bResult = false;
				break;
			}
			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				bool bAny = false;
				if (!DequeueCache.IsFull())
				{
					T* Repush = IncomingQueue.Pop();
					if (!Repush)
					{
						break;
					}
					bResult = false;
					verify(DequeueCache.Push(Repush));
				}
				else
				{
					bResult = false; // we didn't pop one because the dequeue cache is full...guess it isn't empty
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return bResult;
	}
	/**
	*	Check if the list is empty. This is a faster, less rigorous test that can miss a queue full of stuff. Used to optimize thread restarts.
	*/
	bool IsEmptyFast()
	{
		return DequeueCache.IsEmpty() && IncomingQueue.IsEmpty();
	}

	/**
	*	Pop all items from the list.
	*
	*	@param Output The array to hold the returned items. Must be empty.
	*/
	void PopAll(TArray<T *>& Output)
	{
		while (true)
		{
			T* Item = Pop();
			if (!Item)
			{
				break;
			}
			Output.Add(Item);
		}
	}

private:
	FLockFreePointerQueueBaseSingleConsumerIntrusive<T, 0> IncomingQueue;
	FDequeueCache DequeueCache;
	int32 DequeueLock;
};

// FIFO of void *'s
template<int TPaddingForCacheContention>
class FLockFreePointerListFIFOBase : public FNoncopyable
{
public:

	FLockFreePointerListFIFOBase()
		: LinkAllocator(FLockFreePointerQueueBaseLinkAllocator::Get())
		, DequeueLock(0)
	{
	}

	void Push(void *Item)
	{
		FLockFreeLink* Link = LinkAllocator.Alloc();
		Link->Payload = Item;
		IncomingQueue.Push(Link);
	}

	void* Pop(bool bOkToSpeculativelyReturnNull = false)
	{
		int32 SpinCount = 0;
		while (true)
		{
			void* Result = DequeueCache.Pop();
			if (Result)
			{
				return Result;
			}
			if (bOkToSpeculativelyReturnNull && IncomingQueue.IsEmpty())
			{
				return nullptr;
			}
			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				Result = DequeueCache.Pop();
				if (Result)
				{
					// someone beat us to it
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return Result;
				}
				FLockFreeLink* Link = IncomingQueue.Pop();
				if (!Link)
				{
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return nullptr;
				}
				check(!DequeueCache.IsFull() && Link->Payload); // can't be full because it was empty and we have the lock
				verify(DequeueCache.Push(Link->Payload));
				LinkAllocator.Free(Link);
				while (!DequeueCache.IsFull())
				{
					FLockFreeLink* Repush = IncomingQueue.Pop(); 
					if (!Repush)
					{
						break;
					}
					check(Repush->Payload);
					verify(DequeueCache.Push(Repush->Payload));
					LinkAllocator.Free(Repush);
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}

	/**	
	 *	Check if the list is empty.
	 *
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	bool IsEmpty()  
	{
		bool bResult = true;
		int32 SpinCount = 0;
		while (true)
		{
			if (!DequeueCache.IsEmpty())
			{
				bResult = false;
				break;
			}
			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				bool bAny = false;
				if (!DequeueCache.IsFull())
				{
					FLockFreeLink* Repush = IncomingQueue.Pop(); 
					if (!Repush)
					{
						FPlatformMisc::MemoryBarrier();
						DequeueLock = 0;
						break;
					}
					bResult = false;
					check(Repush->Payload);
					verify(DequeueCache.Push(Repush->Payload));
					LinkAllocator.Free(Repush);
				}
				else
				{
					bResult = false; // we didn't pop one because the dequeue cache is full...guess it isn't empty
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return bResult;
	}
	/**	
	 *	Check if the list is empty. This is a faster, less rigorous test that can miss a queue full of stuff. Used to optimize thread restarts.
	 */
	bool IsEmptyFast()  
	{
		return DequeueCache.IsEmpty() && IncomingQueue.IsEmpty();
	}

private:
	FLockFreePointerQueueBaseLinkAllocator& LinkAllocator;
	FLockFreePointerQueueBaseSingleConsumer<TPaddingForCacheContention> IncomingQueue;
	FDequeueCache DequeueCache;
	int32 DequeueLock;
};

// FIFO of void *'s
template<>
class FLockFreePointerListFIFOBase<0> : public FNoncopyable
{
public:

	FLockFreePointerListFIFOBase()
		: LinkAllocator(FLockFreePointerQueueBaseLinkAllocator::Get())
		, DequeueLock(0)
	{
	}

	void Push(void *Item)
	{
		FLockFreeLink* Link = LinkAllocator.Alloc();
		Link->Payload = Item;
		IncomingQueue.Push(Link);
	}

	void* Pop(bool bOkToSpeculativelyReturnNull = false)
	{
		int32 SpinCount = 0;
		while (true)
		{
			void* Result = DequeueCache.Pop();
			if (Result)
			{
				return Result;
			}
			if (bOkToSpeculativelyReturnNull && IncomingQueue.IsEmpty())
			{
				return nullptr;
			}
			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				Result = DequeueCache.Pop();
				if (Result)
				{
					// someone beat us to it
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return Result;
				}
				FLockFreeLink* Link = IncomingQueue.Pop();
				if (!Link)
				{
					FPlatformMisc::MemoryBarrier();
					DequeueLock = 0;
					return nullptr;
				}
				check(!DequeueCache.IsFull() && Link->Payload); // can't be full because it was empty and we have the lock
				verify(DequeueCache.Push(Link->Payload));
				LinkAllocator.Free(Link);
				while (!DequeueCache.IsFull())
				{
					FLockFreeLink* Repush = IncomingQueue.Pop();
					if (!Repush)
					{
						break;
					}
					check(Repush->Payload);
					verify(DequeueCache.Push(Repush->Payload));
					LinkAllocator.Free(Repush);
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
			}
			LockFreeCriticalSpin(SpinCount);
		}
	}

	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	bool IsEmpty()
	{
		bool bResult = true;
		int32 SpinCount = 0;
		while (true)
		{
			if (!DequeueCache.IsEmpty())
			{
				bResult = false;
				break;
			}
			if (!DequeueLock && FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&DequeueLock, 1, 0) == 0)
			{
				TestCriticalStall();
				bool bAny = false;
				if (!DequeueCache.IsFull())
				{
					FLockFreeLink* Repush = IncomingQueue.Pop();
					if (!Repush)
					{
						FPlatformMisc::MemoryBarrier();
						DequeueLock = 0;
						break;
					}
					bResult = false;
					check(Repush->Payload);
					verify(DequeueCache.Push(Repush->Payload));
					LinkAllocator.Free(Repush);
				}
				else
				{
					bResult = false; // we didn't pop one because the dequeue cache is full...guess it isn't empty
				}
				FPlatformMisc::MemoryBarrier();
				DequeueLock = 0;
				break;
			}
			LockFreeCriticalSpin(SpinCount);
		}
		return bResult;
	}
	/**
	*	Check if the list is empty. This is a faster, less rigorous test that can miss a queue full of stuff. Used to optimize thread restarts.
	*/
	bool IsEmptyFast()
	{
		return DequeueCache.IsEmpty() && IncomingQueue.IsEmpty();
	}

private:
	FLockFreePointerQueueBaseLinkAllocator& LinkAllocator;
	FLockFreePointerQueueBaseSingleConsumer<0> IncomingQueue;
	FDequeueCache DequeueCache;
	int32 DequeueLock;
};

// closeable FIFO of void *'s
template<int TPaddingForCacheContention>
class FCloseableLockFreePointerListFIFOBaseSingleConsumer : public FNoncopyable
{
public:

	FCloseableLockFreePointerListFIFOBaseSingleConsumer()
		: LinkAllocator(FLockFreePointerQueueBaseLinkAllocator::Get())
	{
	}
	/**
	 *	Push an item onto the head of the list, unless the list is closed
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL
	 *	@return true if the item was pushed on the list, false if the list was closed.
	 */
	bool PushIfNotClosed(void *NewItem)
	{
		FLockFreeLink* Link = LinkAllocator.Alloc();
		Link->Payload = NewItem;
		if (!IncomingQueue.PushIfNotClosed(Link))
		{
			LinkAllocator.Free(Link);
			return false;
		}
		return true;
	}

	/**	
	 *	Pop all items from the list and atomically close it.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	template<class T>
	void PopAllAndClose(TArray<T *>& Output)
	{
		while (true)
		{
			FLockFreeLink* Link = IncomingQueue.Pop();
			if (Link)
			{
				Output.Add((T*)Link->Payload);
				LinkAllocator.Free(Link);
			}
			else if (IncomingQueue.CloseIfEmpty())
			{
				break;
			}
		}
	}	

	/**	
	 *	Check if the list is closed
	 *
	 *	@return true if the list is closed.
	 */
	FORCEINLINE bool IsClosed() const
	{
		return IncomingQueue.IsClosed();
	}

	/**	
	 *	Push an item onto the head of the list, opening it first if necessary.
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL.
	 *	@return true if the list needed to be opened first, false if the list was not closed before our push.
	 */
	bool ReopenIfClosedAndPush(void *NewItem)
	{
		FLockFreeLink* Link = LinkAllocator.Alloc();
		Link->Payload = NewItem;
		bool bWasReopenedByMe = IncomingQueue.ReopenIfClosedAndPush(Link);
		return bWasReopenedByMe;
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty or closed.
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 *	@return The new item, if any
	 */
	void* Pop()
	{
		FLockFreeLink* Link = IncomingQueue.Pop();
		if (Link)
		{
			void* Result = Link->Payload;
			LinkAllocator.Free(Link);
			return Result;
		}
		return nullptr;
	}

	/**	
	 *	Close the list if it is empty.
	 *
	 *	@return true if this call actively closed the list.
	 */
	bool CloseIfEmpty()
	{
		return IncomingQueue.CloseIfEmpty();
	}

	/**	
	 *	Not thread safe, used to reset the list for recycling without freeing the stub
	 *	@return true if the list is closed.
	 */
	FORCEINLINE void Reset()
	{
		return IncomingQueue.Reset();
	}

private:
	FLockFreePointerQueueBaseLinkAllocator& LinkAllocator;
	FCloseableLockFreePointerQueueBaseSingleConsumer<TPaddingForCacheContention> IncomingQueue;
};

// closeable FIFO of void *'s
template<>
class FCloseableLockFreePointerListFIFOBaseSingleConsumer<0> : public FNoncopyable
{
public:

	FCloseableLockFreePointerListFIFOBaseSingleConsumer()
		: LinkAllocator(FLockFreePointerQueueBaseLinkAllocator::Get())
	{
	}
	/**
	*	Push an item onto the head of the list, unless the list is closed
	*
	*	@param NewItem, the new item to push on the list, cannot be NULL
	*	@return true if the item was pushed on the list, false if the list was closed.
	*/
	bool PushIfNotClosed(void *NewItem)
	{
		FLockFreeLink* Link = LinkAllocator.Alloc();
		Link->Payload = NewItem;
		if (!IncomingQueue.PushIfNotClosed(Link))
		{
			LinkAllocator.Free(Link);
			return false;
		}
		return true;
	}

	/**
	*	Pop all items from the list and atomically close it.
	*
	*	@param Output The array to hold the returned items. Must be empty.
	*/
	template<class T>
	void PopAllAndClose(TArray<T *>& Output)
	{
		while (true)
		{
			FLockFreeLink* Link = IncomingQueue.Pop();
			if (Link)
			{
				Output.Add((T*)Link->Payload);
				LinkAllocator.Free(Link);
			}
			else if (IncomingQueue.CloseIfEmpty())
			{
				break;
			}
		}
	}

	/**
	*	Check if the list is closed
	*
	*	@return true if the list is closed.
	*/
	FORCEINLINE bool IsClosed() const
	{
		return IncomingQueue.IsClosed();
	}

	/**
	*	Push an item onto the head of the list, opening it first if necessary.
	*
	*	@param NewItem, the new item to push on the list, cannot be NULL.
	*	@return true if the list needed to be opened first, false if the list was not closed before our push.
	*/
	bool ReopenIfClosedAndPush(void *NewItem)
	{
		FLockFreeLink* Link = LinkAllocator.Alloc();
		Link->Payload = NewItem;
		bool bWasReopenedByMe = IncomingQueue.ReopenIfClosedAndPush(Link);
		return bWasReopenedByMe;
	}

	/**
	*	Pop an item from the list or return NULL if the list is empty or closed.
	*	CAUTION: This method should not be used unless the list is known to not be closed.
	*	@return The new item, if any
	*/
	void* Pop()
	{
		FLockFreeLink* Link = IncomingQueue.Pop();
		if (Link)
		{
			void* Result = Link->Payload;
			LinkAllocator.Free(Link);
			return Result;
		}
		return nullptr;
	}

	/**
	*	Close the list if it is empty.
	*
	*	@return true if this call actively closed the list.
	*/
	bool CloseIfEmpty()
	{
		return IncomingQueue.CloseIfEmpty();
	}

	/**
	*	Not thread safe, used to reset the list for recycling without freeing the stub
	*	@return true if the list is closed.
	*/
	FORCEINLINE void Reset()
	{
		return IncomingQueue.Reset();
	}

private:
	FLockFreePointerQueueBaseLinkAllocator& LinkAllocator;
	FCloseableLockFreePointerQueueBaseSingleConsumer<0> IncomingQueue;
};


template<class T, int TPaddingForCacheContention>
class TLockFreePointerListFIFO : private FLockFreePointerListFIFOBase<TPaddingForCacheContention>
{
public:
	/**	
	 *	Push an item onto the head of the list.
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL.
	 */
	FORCEINLINE void Push(T *NewItem)
	{
		FLockFreePointerListFIFOBase<TPaddingForCacheContention>::Push(NewItem);
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty.
	 *	@return The popped item, if any.
	 */
	FORCEINLINE T* Pop()
	{
		return (T*)FLockFreePointerListFIFOBase<TPaddingForCacheContention>::Pop();
	}

	/**	
	 *	Pop all items from the list.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	void PopAll(TArray<T *>& Output)
	{
		while (true)
		{
			T* Item = Pop();
			if (!Item)
			{
				break;
			}
			Output.Add(Item);
		}
	}	

	/**	
	 *	Check if the list is empty.
	 *
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	FORCEINLINE bool IsEmpty()  
	{
		return FLockFreePointerListFIFOBase<TPaddingForCacheContention>::IsEmpty();
	}
	/**	
	 *	Check if the list is empty. This is a faster, less rigorous test that can miss a queue full of stuff. Used to optimize thread restarts.
	 */
	FORCEINLINE bool IsEmptyFast()  
	{
		return FLockFreePointerListFIFOBase<TPaddingForCacheContention>::IsEmptyFast();
	}
};

template<class T>
class TLockFreePointerListFIFO<T, 0> : private FLockFreePointerListFIFOBase<0>
{
public:
	/**
	*	Push an item onto the head of the list.
	*
	*	@param NewItem, the new item to push on the list, cannot be NULL.
	*/
	FORCEINLINE void Push(T *NewItem)
	{
		FLockFreePointerListFIFOBase<0>::Push(NewItem);
	}

	/**
	*	Pop an item from the list or return NULL if the list is empty.
	*	@return The popped item, if any.
	*/
	FORCEINLINE T* Pop()
	{
		return (T*)FLockFreePointerListFIFOBase<0>::Pop();
	}

	/**
	*	Pop all items from the list.
	*
	*	@param Output The array to hold the returned items. Must be empty.
	*/
	void PopAll(TArray<T *>& Output)
	{
		while (true)
		{
			T* Item = Pop();
			if (!Item)
			{
				break;
			}
			Output.Add(Item);
		}
	}

	/**
	*	Check if the list is empty.
	*
	*	@return true if the list is empty.
	*	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	*	As typically used, the list is not being access concurrently when this is called.
	*/
	FORCEINLINE bool IsEmpty()
	{
		return FLockFreePointerListFIFOBase<0>::IsEmpty();
	}
	/**
	*	Check if the list is empty. This is a faster, less rigorous test that can miss a queue full of stuff. Used to optimize thread restarts.
	*/
	FORCEINLINE bool IsEmptyFast()
	{
		return FLockFreePointerListFIFOBase<0>::IsEmptyFast();
	}
};

#if USE_NEW_LOCK_FREE_LISTS

// a list where you don't care what order they pop in. We choose the fastest implementation we have.
template<class T, int TPaddingForCacheContention>
class TLockFreePointerListUnordered : public TLockFreePointerListFIFO<T, TPaddingForCacheContention>
{

};

#endif


