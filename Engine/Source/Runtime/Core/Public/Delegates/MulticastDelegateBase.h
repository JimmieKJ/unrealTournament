// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Abstract base class for multicast delegates.
 */
typedef TArray<FDelegateBase, TInlineAllocator<1> > TInvocationList;

template<typename ObjectPtrType>
class FMulticastDelegateBase
{
public:

	~FMulticastDelegateBase()
	{
	}

	/** Removes all functions from this delegate's invocation list. */
	void Clear( )
	{
		for (FDelegateBase& DelegateBaseRef : InvocationList)
		{
			DelegateBaseRef.Unbind();
		}

		CompactInvocationList();
	}

	/**
	 * Checks to see if any functions are bound to this multi-cast delegate.
	 *
	 * @return true if any functions are bound, false otherwise.
	 */
	inline bool IsBound( ) const
	{
		for (const FDelegateBase& DelegateBaseRef : InvocationList)
		{
			if (DelegateBaseRef.GetDelegateInstanceProtected())
			{
				return true;
			}
		}
		return false;
	}

	/** 
	 * Checks to see if any functions are bound to the given user object.
	 *
	 * @return	True if any functions are bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject( void const* InUserObject ) const
	{
		for (const FDelegateBase& DelegateBaseRef : InvocationList)
		{
			IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
			if ((DelegateInstance != nullptr) && DelegateInstance->HasSameObject(InUserObject))
			{
				return true;
			}
		}

		return false;
	}

	/**
	 * Removes all functions from this multi-cast delegate's invocation list that are bound to the specified UserObject.
	 * Note that the order of the delegates may not be preserved!
	 *
	 * @param InUserObject The object to remove all delegates for.
	 */
	void RemoveAll( const void* InUserObject )
	{
		for (int32 InvocationListIndex = InvocationList.Num() - 1; InvocationListIndex >= 0; --InvocationListIndex)
		{
			FDelegateBase& DelegateBaseRef = InvocationList[InvocationListIndex];

			IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
			if ((DelegateInstance != nullptr) && DelegateInstance->HasSameObject(InUserObject))
			{
				DelegateBaseRef.Unbind();
			}
		}

		CompactInvocationList();
	}

protected:

	/** Hidden default constructor. */
	inline FMulticastDelegateBase( )
		: CompactionThreshold(2)
		, InvocationListLockCount(0)
	{ }

protected:

	/**
	 * Adds the given delegate instance to the invocation list.
	 *
	 * @param NewDelegateBaseRef The delegate instance to add.
	 */
	inline FDelegateHandle AddInternal(FDelegateBase&& NewDelegateBaseRef)
	{
		FDelegateHandle Result = NewDelegateBaseRef.GetHandle();
		InvocationList.Add(MoveTemp(NewDelegateBaseRef));
		return Result;
	}

	/**
	 * Removes any expired or deleted functions from the invocation list.
	 *
	 * @see RequestCompaction
	 */
	void CompactInvocationList( )
	{
		if ((InvocationList.Num() < CompactionThreshold) || (InvocationListLockCount != 0))
		{
			return;
		}

		for (int32 InvocationListIndex = InvocationList.Num() - 1; InvocationListIndex >= 0; --InvocationListIndex)
		{
			FDelegateBase& DelegateBaseRef = InvocationList[InvocationListIndex];

			IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();

			if ((DelegateInstance == nullptr) || DelegateInstance->IsCompactable())
			{
				InvocationList.RemoveAtSwap(InvocationListIndex);
			}
		}

		CompactionThreshold = FMath::Max(2, 2 * InvocationList.Num());
	}

	/**
	 * Gets a read-only reference to the invocation list.
	 *
	 * @return The invocation list.
	 */
	inline const TInvocationList& GetInvocationList( ) const
	{
		return InvocationList;
	}

	/** Increments the lock counter for the invocation list. */
	inline void LockInvocationList( ) const
	{
		++InvocationListLockCount;
	}

	/** Decrements the lock counter for the invocation list. */
	inline void UnlockInvocationList( ) const
	{
		--InvocationListLockCount;
	}

protected:
	/**
	 * Helper function for derived classes of FMulticastDelegateBase to get at the delegate instance.
	 */
	static FORCEINLINE IDelegateInstance* GetDelegateInstanceProtectedHelper(const FDelegateBase& Base)
	{
		return Base.GetDelegateInstanceProtected();
	}

private:

	/** Holds the collection of delegate instances to invoke. */
	TInvocationList InvocationList;

	/** Used to determine when a compaction should happen. */
	int32 CompactionThreshold;

	/** Holds a lock counter for the invocation list. */
	mutable int32 InvocationListLockCount;
};
