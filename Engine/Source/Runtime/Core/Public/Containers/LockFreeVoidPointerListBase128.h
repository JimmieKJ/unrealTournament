// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Defines initial values used by the lock free linked list. */
struct FLockFreeListConstants
{
	/**
	 *	Initial ABA counter, this value is greater than 'lower half' of virtual address space in 64-bit addressing.
	 *	It means that you will never get this address in the pointer obtained by new function, mostly for debugging purpose.
	 *	Set to 200000000000000 for easier reading and debugging.
	 */
	static const uint64 InitialABACounter = 0x0000B5E620F48000; //      200000000000000 > 48-bit

	/**
	 *	ABA counter for the special closed link, this value is less than 'higher half' of virtual address space in 64-bit addressing.
	 *	It means that you will never get this address in the pointer obtained by new function, mostly for debugging purpose.
	 */
	static const uint64 SpecialClosedLink = 0xFFFF700000000000; // 18446603336221196288 > 48-bit

	/**
	 * @return next ABA counter value.
	 */
	FORCEINLINE static const uint64 GetNextABACounter()
	{
		static uint64 ABACounter = FLockFreeListConstants::InitialABACounter;
		return FPlatformAtomics::InterlockedIncrement( (volatile int64*)&ABACounter );
	}
};

/*-----------------------------------------------------------------------------
	FLockFreeVoidPointerListBase128
-----------------------------------------------------------------------------*/

MS_ALIGN(16)
class FLockFreeVoidPointerListBase128 : public FNoncopyable
{
	struct FLargePointer;
public:

	/** Constructor - sets the list to empty and initializes the allocator if it hasn't already been initialized. */
	FLockFreeVoidPointerListBase128()
	{
		FLinkAllocator::Get(); // construct allocator singleton if it isn't already
	}

	/** Destructor - checks that the list is either empty or closed. Lists should not be destroyed in any other state. */
	~FLockFreeVoidPointerListBase128()
	{
		// leak these at shutdown
		//checkLockFreePointerList(Head.Pointer == NULL || Head.Pointer == FLinkAllocator::Get().ClosedLink()); // we do not allow destruction except in the empty or closed state
	}

	/**	
	 *	Push an item onto the head of the list
	 *	@param NewItem, the new item to push on the list, cannot be NULL
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	void Push(void *NewItem)
	{
		FLink* NewLink = FLinkAllocator::Get().AllocateLink(NewItem);
		NewLink->Link( Head );
		NumElements.Increment();
		checkLockFreePointerList(NewLink->Next.Pointer != FLinkAllocator::Get().ClosedLink().Pointer); // it is not permissible to call push on a Closed queue
	}

	/**	
	 *	Push an item onto the head of the list, unless the list is closed
	 *	@param NewItem, the new item to push on the list, cannot be NULL
	 *	@return true if the item was pushed on the list, false if the list was closed.
	 */
	bool PushIfNotClosed(void *NewItem)
	{
		FLink* NewLink = FLinkAllocator::Get().AllocateLink(NewItem);
		bool bSuccess = NewLink->LinkIfHeadNotEqual( Head, FLinkAllocator::Get().ClosedLink() );
		if( !bSuccess )
		{
			NewLink->Dispose();
		}
		else
		{
			NumElements.Increment();
		}
		return bSuccess;
	}

	/**	
	 *	Push an item onto the head of the list, opening it first if necessary
	 *	@param NewItem, the new item to push on the list, cannot be NULL
	 *	@return true if the list needed to be opened first, false if the list was not closed before our push
	 */
	bool ReopenIfClosedAndPush(void *NewItem)
	{
		FLink* NewLink = FLinkAllocator::Get().AllocateLink(NewItem);
		while (1)
		{
			if( NewLink->LinkIfHeadNotEqual( Head, FLinkAllocator::Get().ClosedLink() ) )
			{
				NumElements.Increment();
				return false;
			}
			if( NewLink->ReplaceHeadWithThisIfHeadIsClosed( Head ) )
			{
				NumElements.Increment();
				return true;
			}
		}
	}
	
	/**	
	 *	Pop an item from the list or return NULL if the list is empty
	 *	@return The popped item, if any
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	void* Pop()
	{
		FLink* Link = FLink::Unlink( Head );
		if( !Link )
		{
			return nullptr;
		}
		NumElements.Decrement();
		checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink().Pointer); // it is not permissible to call pop on a Closed queue
		void* Return = Link->Item;
		Link->Dispose();
		return Return;
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty or closed
	 *	@return The new item, if any
	 */
	void* PopIfNotClosed()
	{
		FLink* Link = FLink::Unlink( Head, FLinkAllocator::Get().ClosedLink() );
		if( !Link )
		{
			return nullptr;
		}
		NumElements.Decrement();
		checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink().Pointer); // internal error, this should not be possible
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
		if( FLink::ReplaceHeadWithOtherIfHeadIsEmpty( Head, FLinkAllocator::Get().ClosedLink() ) )
		{
			return true;
		}
		return false;
	}

	/**	
	 *	If the list is empty, replace it with the other list and null the other list.
	 *	@return true if this call actively closed the list
	 */
	bool ReplaceListIfEmpty( FLockFreeVoidPointerListBase128& NotThreadSafeTempListToReplaceWith )
	{
		if( FLink::ReplaceHeadWithOtherIfHeadIsEmpty( Head, NotThreadSafeTempListToReplaceWith.Head ) )
		{
			NotThreadSafeTempListToReplaceWith.Head = FLargePointer();
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
		FLink* Link = FLink::ReplaceList( Head );
		NumElements.Set( 0 );
		while (Link)
		{
			checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink().Pointer); // it is not permissible to call PopAll on a Closed queue
			checkLockFreePointerList(Link->Item); // we don't allow null items
			Output.Add((ElementType)(Link->Item));
			FLink* NextLink = Link->Next.Pointer;
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
		FLink* Link = FLink::ReplaceList( Head, FLinkAllocator::Get().ClosedLink() );
		NumElements.Set( 0 );
		while (Link)
		{
			checkLockFreePointerList(Link != FLinkAllocator::Get().ClosedLink().Pointer); // we should not pop off the closed link
			checkLockFreePointerList(Link->Item); // we don't allow null items
			Output.Add((ElementType)(Link->Item));
			FLink* NextLink = Link->Next.Pointer;
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
		return Head.Pointer == FLinkAllocator::Get().ClosedLink().Pointer && Head.ABACounter == FLinkAllocator::Get().ClosedLink().ABACounter;
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
		return Head.Pointer == nullptr && Head.ABACounter == 0;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Checks if item pointers look 'ok'. Used for debugging.
	 */
	bool CheckPointers() const
	{
		const int32 AlignmentCheck = ELockFreeAligment::LF128bit - 1;
		FPlatformMisc::MemoryBarrier();

		for( FLink* Link = Head.Pointer; Link; Link = Link->Next.Pointer )
		{
			void* ItemPtr = Link->Item;
			if (!ItemPtr || (UPTRINT)ItemPtr < 0x100 || (UPTRINT)ItemPtr > FLockFreeListConstants::InitialABACounter )
			{
				return false;
			}	
			if ((UPTRINT)ItemPtr & AlignmentCheck)
			{
				return false;
			}

			if ((UPTRINT)Link & AlignmentCheck)
			{
				return false;
			}

			if( Link->Next.ABACounter != 0 && Link->Next.ABACounter < FLockFreeListConstants::InitialABACounter )
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * @return number of elements via iterating through this list, used for debugging.
	 */
	int32 NumVerified() const
	{
		FPlatformMisc::MemoryBarrier();

		int32 NumElements = 0;
		for( FLink* Link = Head.Pointer; Link; Link = Link->Next.Pointer )
		{
			NumElements++;
		}
		return NumElements;
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

private:
	struct FLink;

	/** Fakes 128-bit pointers to solve the ABA problem. */
	MS_ALIGN(16)
	struct FLargePointer
	{
		/** Default constructor, initializes to zero. */
		FLargePointer()
			: Pointer( nullptr )
			, ABACounter( 0 )
		{
		}

		/**
		 *	Initialization constructor, only used to initialize the SpecialClosedLink
		 */
		FLargePointer( FLink* InPointer, uint64 InABACounter )
			: Pointer( InPointer )
			, ABACounter( InABACounter )
		{
		}

		/**
		 *	Initialization constructor, only used to get the current value of the head.
		 *	Read atomically 8-bytes and sets ABACounter to 0, which is invalid.
		 */
		FLargePointer( FLink* InPointer  )
			: Pointer( InPointer )
			, ABACounter( 0 )
		{
		}

		/** Copy constructor, only valid if copied from constant. */
		FLargePointer( const FLargePointer& Other )
		{
			Pointer = Other.Pointer;
			ABACounter = Other.ABACounter;
		}

		/** Holds pointer to the element. */
		FLink* Pointer;
		/** Holds ABA counter. */
		uint64 ABACounter;
	}
	GCC_ALIGN(16);

	/**
	 *	Internal single-linked list link structure.
	 *	32 bytes, 8 bytes wasted, not used
	 */
	MS_ALIGN(16) 
	struct FLink : public FNoncopyable
	{
		/** 16 bytes. 'Pointer' to the next element. */
		FLargePointer Next;

		/** 8 bytes. Pointer to the user data. */
		void* Item;

		FLink()
			: Item( nullptr )
		{}

		FLargePointer GetSafeNewLinkPointer()
		{
			return FLargePointer( this, FLockFreeListConstants::GetNextABACounter() );	
		}

		/**	
		 *	Link this node into the head of the given list.
		 *	@param Head; the head of the list
		 *	CAUTION: Not checked here, but linking into a closed list with the routine would accidentally and incorrectly open the list
		 */
		void Link(FLargePointer& HeadPointer)
		{
			const FLargePointer NewLinkPointer = GetSafeNewLinkPointer();
			FLargePointer LocalHeadPointer     = FLargePointer(HeadPointer.Pointer);

			do
			{
				Next = LocalHeadPointer;
			}
			while( FPlatformAtomics::InterlockedCompareExchange128( (volatile FInt128*)&HeadPointer, (FInt128&)NewLinkPointer, (FInt128*)&LocalHeadPointer ) == false );
		}

		/**	
		 *	Link this node into the head of the given list, unless the head is a special, given node
		 *	@param HeadPointer; the head of the list
		 *	@param SpecialClosedLink; special link that is never recycled that indicates a closed list.
		 *	@return true if this node was linked to the head.
		 */
		bool LinkIfHeadNotEqual( FLargePointer& HeadPointer, const FLargePointer& SpecialClosedLink )
		{
			checkLockFreePointerList(!Next.Pointer); // this should not be set yet

			const FLargePointer NewLinkPointer = GetSafeNewLinkPointer();
			FLargePointer LocalHeadPointer     = FLargePointer(HeadPointer.Pointer);

			do
			{
				if( LocalHeadPointer.Pointer==SpecialClosedLink.Pointer )
				{
					Next = FLargePointer();
					return false;
				}

				Next = LocalHeadPointer;
			}
			while( FPlatformAtomics::InterlockedCompareExchange128( (volatile FInt128*)&HeadPointer, (FInt128&)NewLinkPointer, (FInt128*)&LocalHeadPointer ) == false );

			return true;
		}

		/**	
		 *	If the head is a closed list, then replace it with this node. This is the primitive that is used to open lists
		 *	@param HeadPointer; the head of the list
		 *	@return true if head was replaced with this node
		 *	CAUTION: The test link must a special link that is never recycled. The ABA problem is assumed to not occur here.
		 */
		bool ReplaceHeadWithThisIfHeadIsClosed( const FLargePointer& HeadPointer )
		{
			const FLargePointer NewLinkPointer    = GetSafeNewLinkPointer();
			const FLargePointer ClosedLinkPointer = FLinkAllocator::Get().ClosedLink();
			FLargePointer LocalHeadPointer        = FLinkAllocator::Get().ClosedLink();

			do
			{
				// This will be the same on the first iteration, then LocalHeadPointer will be updated with the latest HeadPointer.
				if( LocalHeadPointer.Pointer != ClosedLinkPointer.Pointer )
				{
					return false;
				}
			}
			while( FPlatformAtomics::InterlockedCompareExchange128( (volatile FInt128*)&HeadPointer, (FInt128&)NewLinkPointer, (FInt128*)&LocalHeadPointer ) == false );

			return true;
		}

		/**	
		 *	If the head is empty, then replace it with a head from the second list. This is the primitive that is used to close lists
		 *	@param HeadPointer; the head of the list
		 *	@param HeadToReplace; the head of the list to be replaced
		 *	@return true if head was replaced with a new head
		 */
		static bool ReplaceHeadWithOtherIfHeadIsEmpty( FLargePointer& HeadPointer, const FLargePointer& HeadToReplace )
		{
			const FLargePointer LocalHeadToReplace = HeadToReplace;
			const FLargePointer LocalEmptyLink  = FLargePointer();
			FLargePointer LocalHeadPointer      = FLargePointer();

			do
			{
				// This will be the same on the first iteration, then LocalHeadPointer will be updated with the latest HeadPointer.
				if( LocalHeadPointer.Pointer != LocalEmptyLink.Pointer )
				{
					return false;
				}
			}
			while( FPlatformAtomics::InterlockedCompareExchange128( (volatile FInt128*)&HeadPointer, (FInt128&)LocalHeadToReplace, (FInt128*)&LocalHeadPointer ) == false );

			return true;
		}

		/**	
		 *	Pop an item off the list, unless the list is empty or the head is a special, given node
		 *	@param HeadPointer; the head of the list
		 *	@param SpecialClosedLink; special link that is never recycled that indicates a closed list. Can be NULL for lists that are known to not be closed
		 *	@return The link that was popped off, or NULL if the list was empty or closed.
		 *	CAUTION: Not checked here, but if the list is closed and SpecialClosedLink is NULL, you will pop off the closed link, and that usually isn't what you want
		 */
		static FLink* Unlink( FLargePointer& HeadPointer, const FLargePointer& SpecialClosedLink = FLargePointer() )
		{
			FLargePointer LocalHeadPointer = FLargePointer(HeadPointer.Pointer);
			FLargePointer NextPointer;

			do
			{
				if( LocalHeadPointer.Pointer==nullptr )
				{
					return nullptr;
				}
				if( LocalHeadPointer.Pointer==SpecialClosedLink.Pointer )
				{
					return nullptr;
				}

				//NextPointer = FLargePointer( LocalHeadPointer.Pointer->Next.Pointer, FLockFreeListConstants::GetNextABACounter() );	
				NextPointer = LocalHeadPointer.Pointer->Next;
			}
			while( FPlatformAtomics::InterlockedCompareExchange128( (volatile FInt128*)&HeadPointer, (FInt128&)NextPointer, (FInt128*)&LocalHeadPointer ) == false );
			checkLockFreePointerList(NextPointer.Pointer == LocalHeadPointer.Pointer->Next.Pointer);

			return LocalHeadPointer.Pointer;
		}

		/**	
		 *	Replace the list with another list. Use to either acquire all of the items or acquire all of the items and close the list.
		 *	@param HeadPointer; the head of the list
		 *	@param NewHeadLink; Head of the new list to replace this list with
		 *	@return The original list before we replaced it.
		 *	CAUTION: Not checked here, but if the list is closed this is probably not what you want. 
		 */
		static FLink* ReplaceList( FLargePointer& HeadPointer, const FLargePointer& NewHeadLink = FLargePointer() )
		{
			FLargePointer LocalHeadPointer         = FLargePointer(HeadPointer.Pointer);
			const FLargePointer NewHeadLinkPointer = NewHeadLink;

			do 
			{
				if (LocalHeadPointer.Pointer==nullptr && NewHeadLink.Pointer==nullptr)
				{
					// we are asking to replace NULL with NULL, this does not require an atomic (or any) operation
					return nullptr;
				}

				checkLockFreePointerList(LocalHeadPointer.Pointer != NewHeadLink.Pointer); // replacing NULL with NULL is ok, but otherwise we cannot be replacing something with itself or we lose determination of who did what.
			} 
			while( FPlatformAtomics::InterlockedCompareExchange128( (volatile FInt128*)&HeadPointer, (FInt128&)NewHeadLinkPointer, (FInt128*)&LocalHeadPointer ) == false );

			return LocalHeadPointer.Pointer;
		}

		/**
		 *	Disposes this node.
		 */
		void Dispose();
	}
	GCC_ALIGN(16);

	/**
	 *	Class to allocate and recycle links. 
	 */
	MS_ALIGN(16) 
	class FLinkAllocator
	{
	public:
		/**	
		 *	Return a new link.
		 *	@param NewItem "item" pointer of the new node.
		 *	@return New node, ready for use
		 */
		FLink* AllocateLink( void *NewItem )
		{
			checkLockFreePointerList(NewItem); // we don't allow null items
			if (NumUsedLinks.Increment() % 8192 == 1)
			{
				UE_CLOG(0/*MONITOR_LINK_ALLOCATION*/,LogLockFreeList, Log, TEXT("Number of links %d"),NumUsedLinks.GetValue());
			}
	
			FLink* NewLink = FLink::Unlink( FreeLinks );
			if( NewLink )
			{
				NewLink->Next = FLargePointer();
				NewLink->Item = nullptr;
				NumFreeLinks.Decrement();
			}
			else
			{
				if (NumAllocatedLinks.Increment() % 10 == 1)
				{
					UE_CLOG(0/*MONITOR_LINK_ALLOCATION*/,LogLockFreeList, Log, TEXT("Number of allocated links %d"),NumAllocatedLinks.GetValue());
				}

				NewLink = new FLink();
			}
	
			checkLockFreePointerList(!NewLink->Item);
			checkLockFreePointerList(!NewLink->Next.Pointer);
			NewLink->Item = NewItem;
			FPlatformMisc::MemoryBarrier();
			return NewLink;
		}	
		/**	
		 *	Make a link available for recycling. 
		 *	@param Link link to recycle
		 *	CAUTION: Do not call this directly, it should only be called when the reference count drop to zero.
		 */
		void FreeLink( FLink* Link )
		{
			checkLockFreePointerList(Link != ClosedLink().Pointer);  // very bad to recycle the special link
			NumUsedLinks.Decrement();
			FPlatformMisc::MemoryBarrier();
			Link->Link(FreeLinks);
			NumFreeLinks.Increment();
		}
		/**
		 *	@return a pointer to special closed link 
		 */
		const FLargePointer& ClosedLink() const
		{
			checkLockFreePointerList(NULL != SpecialClosedLink.Pointer);
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
	
		/** Head to a list of free links that can be used for new allocations. **/
		FLargePointer	FreeLinks;
	
		/** Pointer to the special closed link. It should never be recycled. **/
		FLargePointer	SpecialClosedLink;
	
		/** Total number of links outstanding and not in the free list **/
		FLockFreeListCounter	NumUsedLinks; 
		/** Total number of links in the free list **/
		FLockFreeListCounter	NumFreeLinks;
		/** Total number of links allocated **/
		FLockFreeListCounter	NumAllocatedLinks; 
	}
	GCC_ALIGN(16);

	/** Head of the list. */
	FLargePointer Head;

	/**
	 *	Current number of elements, due to the high contention this value may not be valid. 
	 *	Used for debugging.
	 */
	FLockFreeListCounter NumElements;
};

inline void FLockFreeVoidPointerListBase128::FLink::Dispose()
{
	Next = FLockFreeVoidPointerListBase128::FLargePointer();
	Item = nullptr;

	FPlatformMisc::MemoryBarrier(); // memory barrier in here to make sure item and next are cleared before the link is recycled

	FLockFreeVoidPointerListBase128::FLinkAllocator::Get().FreeLink( this );
}

typedef FLockFreeVoidPointerListBase128	FLockFreeVoidPointerListGeneric;
