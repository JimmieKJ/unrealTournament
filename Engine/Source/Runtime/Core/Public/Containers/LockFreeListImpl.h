// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** 
 *	Wrapper around FLockFreeVoidPointerListBase
 *	Simple lock free list (stack) that does not permit closure.
 *	T is the type of the pointer the list will contain.
 */
template <class T>
class TLockFreePointerList : private FLockFreeVoidPointerListGeneric 
{
public:
	/**	
	 *	Push an item onto the head of the list.
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL.
	 */
	void Push(T *NewItem)
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		FLockFreeVoidPointerListGeneric::Push(NewItem);
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty.
	 *	@return The popped item, if any.
	 */
	T* Pop()
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		return (T*)FLockFreeVoidPointerListGeneric::Pop();
	}

	/**	
	 *	Pop all items from the list.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	void PopAll(TArray<T *>& Output)
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		FLockFreeVoidPointerListGeneric::PopAll<TArray<T *>, T * >(Output);
	}	

	/**	
	 *	If the list is empty, replace it with the other list and null the other list.
	 *	@return true if this call actively closed the list
	 */
	bool ReplaceListIfEmpty(TLockFreePointerList<T>& NotThreadSafeTempListToReplaceWith)
	{
		return FLockFreeVoidPointerListGeneric::ReplaceListIfEmpty(NotThreadSafeTempListToReplaceWith);
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
		return FLockFreeVoidPointerListGeneric::IsEmpty();
	}

#if	!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Checks if the pointers look 'ok'. Used for debugging.
	 */
	bool CheckPointers() const
	{
		return FLockFreeVoidPointerListGeneric::CheckPointers();
	}

	/**
	 * @return number of elements via iterating through this list, used for debugging.
	 */
	int32 NumVerified() const
	{
		return FLockFreeVoidPointerListGeneric::NumVerified();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

};

template <class T>
class TClosableLockFreePointerList : private FLockFreeVoidPointerListGeneric 
{
public:
	/**	
	 *	Push an item onto the head of the list, unless the list is closed
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL
	 *	@return true if the item was pushed on the list, false if the list was closed.
	 */
	bool PushIfNotClosed(T *NewItem)
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		return FLockFreeVoidPointerListGeneric::PushIfNotClosed(NewItem);
	}

	/**	
	 *	Pop all items from the list and atomically close it.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	void PopAllAndClose(TArray<T *>& Output)
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		FLockFreeVoidPointerListGeneric::PopAllAndClose<TArray<T *>, T * >(Output);
	}	

	/**	
	 *	Check if the list is closed
	 *
	 *	@return true if the list is closed.
	 */
	bool IsClosed() const
	{
		return FLockFreeVoidPointerListGeneric::IsClosed();
	}

#if	!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Checks if the pointers look 'ok'. Used for debugging.
	 */
	bool CheckPointers() const
	{
		return FLockFreeVoidPointerListGeneric::CheckPointers();
	}

	/**
	 * @return number of elements via iterating through this list, used for debugging.
	 */
	int32 NumVerified() const
	{
		return FLockFreeVoidPointerListGeneric::NumVerified();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
};

template <class T>
class TReopenableLockFreePointerList : private FLockFreeVoidPointerListGeneric
{
public:
	/**	
	 *	Push an item onto the head of the list, opening it first if necessary.
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL.
	 *	@return true if the list needed to be opened first, false if the list was not closed before our push.
	 */
	bool ReopenIfClosedAndPush(T *NewItem)
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		bool bWasReopenedByMe = FLockFreeVoidPointerListGeneric::ReopenIfClosedAndPush(NewItem);
		return bWasReopenedByMe;
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty or closed.
	 *	@return The new item, if any
	 */
	T* PopIfNotClosed()
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		return (T*)FLockFreeVoidPointerListGeneric::PopIfNotClosed();
	}

	/**	
	 *	Pop all items from the list.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	void PopAll(TArray<T *>& Output)
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		FLockFreeVoidPointerListGeneric::PopAll<TArray<T *>, T * >(Output);
	}	
	/**	
	 *	Close the list if it is empty.
	 *
	 *	@return true if this call actively closed the list.
	 */
	bool CloseIfEmpty()
	{
		FLockFreeListPerfCounter LockFreePerfCounter;
		return FLockFreeVoidPointerListGeneric::CloseIfEmpty();
	}

#if	!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Checks if the pointers look 'ok'. Used for debugging.
	 */
	bool CheckPointers() const
	{
		return FLockFreeVoidPointerListGeneric::CheckPointers();
	}

	/**
	 * @return number of elements via iterating through this list, used for debugging.
	 */
	int32 NumVerified() const
	{
		return FLockFreeVoidPointerListGeneric::NumVerified();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
};
