// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/** 
 *	Wrapper around FLockFreeVoidPointerListBase
 *	Simple lock free list (stack) that does not permit closure.
 *	T is the type of the pointer the list will contain.
 */
template <class T>
class TLockFreePointerListLIFOBase : private FLockFreeVoidPointerListGeneric 
{
public:
	/**	
	 *	Push an item onto the head of the list.
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL.
	 */
	void Push(T *NewItem)
	{
		FLockFreeVoidPointerListGeneric::Push(NewItem);
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty.
	 *	@return The popped item, if any.
	 */
	T* Pop()
	{
		return (T*)FLockFreeVoidPointerListGeneric::Pop();
	}

	/**	
	 *	Pop all items from the list.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	void PopAll(TArray<T *>& Output)
	{
		FLockFreeVoidPointerListGeneric::PopAll<TArray<T *>, T * >(Output);
	}	

	/**	
	 *	Check if the list is empty.
	 *
	 *	@return true if the list is empty.
	 *	CAUTION: This methods safety depends on external assumptions. For example, if another thread could add to the list at any time, the return value is no better than a best guess.
	 *	As typically used, the list is not being access concurrently when this is called.
	 */
	FORCEINLINE bool IsEmpty() const  
	{
		return FLockFreeVoidPointerListGeneric::IsEmpty();
	}
protected:
	/**	
	 *	If the list is empty, replace it with the other list and null the other list.
	 *	@return true if this call actively closed the list
	 */
	FORCEINLINE bool ReplaceListIfEmpty(TLockFreePointerListLIFOBase<T>& NotThreadSafeTempListToReplaceWith)
	{
		return FLockFreeVoidPointerListGeneric::ReplaceListIfEmpty(NotThreadSafeTempListToReplaceWith);
	}
};

template <class T>
class TLockFreePointerListLIFO : public TLockFreePointerListLIFOBase<T> 
{
public:
	FORCEINLINE bool ReplaceListIfEmpty(TLockFreePointerListLIFO<T>& NotThreadSafeTempListToReplaceWith)
	{
		return TLockFreePointerListLIFOBase<T>::ReplaceListIfEmpty(NotThreadSafeTempListToReplaceWith);
	}

};

template <class T>
class TClosableLockFreePointerListLIFO : private FLockFreeVoidPointerListGeneric 
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
		return FLockFreeVoidPointerListGeneric::PushIfNotClosed(NewItem);
	}

	/**	
	 *	Pop all items from the list and atomically close it.
	 *
	 *	@param Output The array to hold the returned items. Must be empty.
	 */
	void PopAllAndClose(TArray<T *>& Output)
	{
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
};

template <class T>
class TReopenableLockFreePointerListLIFO : private FLockFreeVoidPointerListGeneric
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
		bool bWasReopenedByMe = FLockFreeVoidPointerListGeneric::ReopenIfClosedAndPush(NewItem);
		return bWasReopenedByMe;
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty or closed.
	 *	@return The new item, if any
	 */
	T* PopIfNotClosed()
	{
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
		FLockFreeVoidPointerListGeneric::PopAll<TArray<T *>, T * >(Output);
	}	
	/**	
	 *	Close the list if it is empty.
	 *
	 *	@return true if this call actively closed the list.
	 */
	bool CloseIfEmpty()
	{
		return FLockFreeVoidPointerListGeneric::CloseIfEmpty();
	}
};

#if !USE_NEW_LOCK_FREE_LISTS
// where you don't care what order they pop in. We choose the fastest implementation we have.
template<class T, int TPaddingForCacheContention>
class TLockFreePointerListUnordered : public TLockFreePointerListLIFOBase<T>
{

};
template<class T, int TPaddingForCacheContention>
class TClosableLockFreePointerListUnorderedSingleConsumer : public TClosableLockFreePointerListLIFO<T>
{

};

#else

template<class T, int TPaddingForCacheContention>
class TClosableLockFreePointerListFIFOSingleConsumer
{
public:
	/**	
	 *	Push an item onto the head of the list, unless the list is closed
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL
	 *	@return true if the item was pushed on the list, false if the list was closed.
	 */
	FORCEINLINE bool PushIfNotClosed(T *NewItem)
	{
		return Inner.PushIfNotClosed(NewItem);
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty
	 *	@return The new item, if any
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	FORCEINLINE T* Pop()
	{
		return (T*)Inner.Pop();
	}

	/**	
	 *	Close the list if it is empty.
	 *	@return true if this call actively closed the list.
	 */
	FORCEINLINE bool CloseIfEmpty()
	{
		return Inner.CloseIfEmpty();
	}

	/**	
	 *	Check if the list is closed
	 *	@return true if the list is closed.
	 */
	FORCEINLINE bool IsClosed() const
	{
		return Inner.IsClosed();
	}
	/**	
	 *	Not thread safe, used to reset the list for recycling without freeing the stub
	 *	@return true if the list is closed.
	 */
	FORCEINLINE void Reset()
	{
		return Inner.Reset();
	}
private:
	FCloseableLockFreePointerListFIFOBaseSingleConsumer<TPaddingForCacheContention> Inner;
};

template<class T, int TPaddingForCacheContention>
class TReopenableLockFreePointerListFIFOSingleConsumer
{
public:
	/**	
	 *	Push an item onto the head of the list, opening it first if necessary.
	 *
	 *	@param NewItem, the new item to push on the list, cannot be NULL.
	 *	@return true if the list needed to be opened first, false if the list was not closed before our push.
	 */
	FORCEINLINE bool ReopenIfClosedAndPush(T *NewItem)
	{
		return Inner.ReopenIfClosedAndPush(NewItem);
	}

	/**	
	 *	Pop an item from the list or return NULL if the list is empty
	 *	@return The new item, if any
	 *	CAUTION: This method should not be used unless the list is known to not be closed.
	 */
	FORCEINLINE T* Pop()
	{
		return (T*)Inner.Pop();
	}

	/**	
	 *	Close the list if it is empty.
	 *	@return true if this call actively closed the list.
	 */
	FORCEINLINE bool CloseIfEmpty()
	{
		return Inner.CloseIfEmpty();
	}
private:
	FCloseableLockFreePointerListFIFOBaseSingleConsumer<TPaddingForCacheContention> Inner;
};

template<class T, int TPaddingForCacheContention>
class TClosableLockFreePointerListUnorderedSingleConsumer : public TClosableLockFreePointerListFIFOSingleConsumer<T, TPaddingForCacheContention>
{

};


#endif

