// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Enumerates available event pool types.
 */
enum class EEventPoolTypes
{
	/** Creates events that have their signaled state reset automatically. */
	AutoReset,

	/** Creates events that have their signaled state reset manually. */
	ManualReset
};

/**
 * Template class for event pools.
 *
 * Events are expensive to create on most platforms. This pool allows for efficient
 * recycling of event instances that are no longer used. Events can have their signaled
 * state reset automatically or manually. The PoolType template parameter specifies
 * which type of events the pool managers.
 *
 * @param PoolType Specifies the type of pool.
 * @see FEvent
 */
template<EEventPoolTypes PoolType>
class FEventPool
{
public:

	/**
	 * Gets the singleton instance of the event pool.
	 *
	 * @return Pool singleton.
	 */
	CORE_API static FEventPool& Get()
	{
		static FEventPool Singleton;
		return Singleton;
	}

	/**
	 * Gets an event from the pool or creates one if necessary.
	 *
	 * @return The event.
	 * @see ReturnToPool
	 */
	FEvent* GetEventFromPool()
	{
		FEvent* Result = Pool.Pop();

		if (!Result)
		{
			// FEventPool is allowed to create synchronization events.
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Result = FPlatformProcess::CreateSynchEvent((PoolType == EEventPoolTypes::ManualReset));
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		Result->AdvanceStats();

		check(Result);
		return Result;
	}

	/**
	 * Returns an event to the pool.
	 *
	 * @param Event The event to return.
	 * @see GetEventFromPool
	 */
	void ReturnToPool(FEvent* Event)
	{
		check(Event);
		check(Event->IsManualReset() == (PoolType == EEventPoolTypes::ManualReset));

		Event->Reset();

		Pool.Push(Event);
	}

private:

	/** Holds the collection of recycled events. */
	TLockFreePointerListUnordered<FEvent, PLATFORM_CACHE_LINE_SIZE> Pool;
};
