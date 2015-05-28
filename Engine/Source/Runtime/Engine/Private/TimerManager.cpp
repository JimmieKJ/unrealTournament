// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TickTaskManager.cpp: Manager for ticking tasks
=============================================================================*/

#include "EnginePrivate.h"

void FTimerHandle::MakeValid()
{
	static int LastAssignedHandle = -1;

	if (!IsValid())
	{
		++LastAssignedHandle;
		Handle = LastAssignedHandle;
	}

	check(IsValid());
}


inline bool DEPRECATED_CompareUnifiedDelegates(const FTimerUnifiedDelegate& Lhs, const FTimerUnifiedDelegate& Rhs)
{
	return ( Lhs.FuncDelegate.IsBound() && (Lhs.FuncDelegate.DEPRECATED_Compare(Rhs.FuncDelegate)) ) || ( Lhs.FuncDynDelegate.IsBound() && (Lhs.FuncDynDelegate == Rhs.FuncDynDelegate) );
}

// ---------------------------------
// Private members
// ---------------------------------

/** Will find and return a timer if it exists, regardless whether it is paused. */ 
FTimerData const* FTimerManager::DEPRECATED_FindTimer(FTimerUnifiedDelegate const& InDelegate, int32* OutTimerIndex) const
{
	if (!CurrentlyExecutingTimer.TimerHandle.IsValid() && DEPRECATED_CompareUnifiedDelegates(InDelegate, CurrentlyExecutingTimer.TimerDelegate))
	{
		// found it currently executing
		if (OutTimerIndex)
		{
			*OutTimerIndex = -1;
		}
		return &CurrentlyExecutingTimer;
	}

	int32 ActiveTimerIdx = DEPRECATED_FindTimerInList(ActiveTimerHeap, InDelegate);
	if (ActiveTimerIdx != INDEX_NONE)
	{
		// found it in the active heap
		if (OutTimerIndex)
		{
			*OutTimerIndex = ActiveTimerIdx;
		}
		return &ActiveTimerHeap[ActiveTimerIdx];
	}

	int32 PausedTimerIdx = DEPRECATED_FindTimerInList(PausedTimerList, InDelegate);
	if (PausedTimerIdx != INDEX_NONE)
	{
		// found it in the paused list
		if (OutTimerIndex)
		{
			*OutTimerIndex = PausedTimerIdx;
		}
		return &PausedTimerList[PausedTimerIdx];
	}

	int32 PendingTimerIdx = DEPRECATED_FindTimerInList(PendingTimerList, InDelegate);
	if (PendingTimerIdx != INDEX_NONE)
	{
		// found it in the pending list
		if (OutTimerIndex)
		{
			*OutTimerIndex = PendingTimerIdx;
		}
		return &PendingTimerList[PendingTimerIdx];
	}

	return nullptr;
}

FTimerData const* FTimerManager::FindTimer(FTimerHandle const& InHandle, int32* OutTimerIndex) const
{
	if (InHandle.IsValid())
	{
		if (CurrentlyExecutingTimer.TimerHandle == InHandle)
		{
			// found it currently executing
			if (OutTimerIndex)
			{
				*OutTimerIndex = -1;
			}
			return &CurrentlyExecutingTimer;
		}

		int32 ActiveTimerIdx = FindTimerInList(ActiveTimerHeap, InHandle);
		if (ActiveTimerIdx != INDEX_NONE)
		{
			// found it in the active heap
			if (OutTimerIndex)
			{
				*OutTimerIndex = ActiveTimerIdx;
			}
			return &ActiveTimerHeap[ActiveTimerIdx];
		}

		int32 PausedTimerIdx = FindTimerInList(PausedTimerList, InHandle);
		if (PausedTimerIdx != INDEX_NONE)
		{
			// found it in the paused list
			if (OutTimerIndex)
			{
				*OutTimerIndex = PausedTimerIdx;
			}
			return &PausedTimerList[PausedTimerIdx];
		}

		int32 PendingTimerIdx = FindTimerInList(PendingTimerList, InHandle);
		if (PendingTimerIdx != INDEX_NONE)
		{
			// found it in the pending list
			if (OutTimerIndex)
			{
				*OutTimerIndex = PendingTimerIdx;
			}
			return &PendingTimerList[PendingTimerIdx];
		}
	}

	return nullptr;
}


/** Will find the given timer in the given TArray and return its index. */ 
int32 FTimerManager::DEPRECATED_FindTimerInList(const TArray<FTimerData> &SearchArray, FTimerUnifiedDelegate const& InDelegate) const
{
	for (int32 Idx=0; Idx<SearchArray.Num(); ++Idx)
	{
		if (!SearchArray[Idx].TimerHandle.IsValid() && DEPRECATED_CompareUnifiedDelegates(SearchArray[Idx].TimerDelegate, InDelegate))
		{
			return Idx;
		}
	}

	return INDEX_NONE;
}

/** Will find the given timer in the given TArray and return its index. */
int32 FTimerManager::FindTimerInList(const TArray<FTimerData> &SearchArray, FTimerHandle const& InHandle) const
{
	if (InHandle.IsValid())
	{
		for (int32 Idx = 0; Idx < SearchArray.Num(); ++Idx)
		{
			if (SearchArray[Idx].TimerHandle == InHandle)
			{
				return Idx;
			}
		}
	}

	return INDEX_NONE;
}

/** Finds a handle to a dynamic timer bound to a particular pointer and function name. */
FTimerHandle FTimerManager::K2_FindDynamicTimerHandle(FTimerDynamicDelegate InDynamicDelegate) const
{
	if (CurrentlyExecutingTimer.TimerDelegate.FuncDynDelegate == InDynamicDelegate)
	{
		return CurrentlyExecutingTimer.TimerHandle;
	}

	if (auto* Timer = ActiveTimerHeap.FindByPredicate([=](const FTimerData& Data){ return Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate; }))
	{
		return Timer->TimerHandle;
	}

	if (auto* Timer = PausedTimerList.FindByPredicate([=](const FTimerData& Data){ return Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate; }))
	{
		return Timer->TimerHandle;
	}

	if (auto* Timer = PendingTimerList.FindByPredicate([=](const FTimerData& Data){ return Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate; }))
	{
		return Timer->TimerHandle;
	}

	return FTimerHandle();
}


void FTimerManager::DEPRECATED_InternalSetTimer(FTimerUnifiedDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay)
{
	// not currently threadsafe
	check(IsInGameThread());

	// if the timer is already set, just clear it and we'll re-add it, since 
	// there's no data to maintain.
	DEPRECATED_InternalClearTimer(InDelegate);

	if (InRate > 0.f)
	{
		// set up the new timer
		FTimerData NewTimerData;
		NewTimerData.TimerDelegate = InDelegate;

		InternalSetTimer(NewTimerData, InRate, InbLoop, InFirstDelay);
	}
}

void FTimerManager::InternalSetTimer(FTimerHandle& InOutHandle, FTimerUnifiedDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay)
{
	// not currently threadsafe
	check(IsInGameThread());

	if (InOutHandle.IsValid())
	{
		// if the timer is already set, just clear it and we'll re-add it, since 
		// there's no data to maintain.
		InternalClearTimer(InOutHandle);
	}

	if (InRate > 0.f)
	{
		InOutHandle.MakeValid();

		// set up the new timer
		FTimerData NewTimerData;
		NewTimerData.TimerHandle = InOutHandle;
		NewTimerData.TimerDelegate = InDelegate;

		InternalSetTimer(NewTimerData, InRate, InbLoop, InFirstDelay);
	}
}

void FTimerManager::InternalSetTimer(FTimerData& NewTimerData, float InRate, bool InbLoop, float InFirstDelay)
{
	if (NewTimerData.TimerHandle.IsValid() || NewTimerData.TimerDelegate.IsBound())
	{
		NewTimerData.Rate = InRate;
		NewTimerData.bLoop = InbLoop;
		NewTimerData.bRequiresDelegate = NewTimerData.TimerDelegate.IsBound();

		const float FirstDelay = (InFirstDelay >= 0.f) ? InFirstDelay : InRate;

		if (HasBeenTickedThisFrame())
		{
			NewTimerData.ExpireTime = InternalTime + FirstDelay;
			NewTimerData.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush(NewTimerData);
		}
		else
		{
			// Store time remaining in ExpireTime while pending
			NewTimerData.ExpireTime = FirstDelay;
			NewTimerData.Status = ETimerStatus::Pending;
			PendingTimerList.Add(NewTimerData);
		}
	}
}

void FTimerManager::InternalSetTimerForNextTick(FTimerUnifiedDelegate const& InDelegate)
{
	// not currently threadsafe
	check(IsInGameThread());

	FTimerData NewTimerData;
	NewTimerData.Rate = 0.f;
	NewTimerData.bLoop = false;
	NewTimerData.bRequiresDelegate = true;
	NewTimerData.TimerDelegate = InDelegate;
	NewTimerData.ExpireTime = InternalTime;
	NewTimerData.Status = ETimerStatus::Active;
	ActiveTimerHeap.HeapPush(NewTimerData);
}

void FTimerManager::DEPRECATED_InternalClearTimer(FTimerUnifiedDelegate const& InDelegate)
{
	// not currently threadsafe
	check(IsInGameThread());

	int32 TimerIdx;
	FTimerData const* const TimerData = DEPRECATED_FindTimer(InDelegate, &TimerIdx);
	if (TimerData)
	{
		InternalClearTimer(TimerIdx, TimerData->Status);
	}
}

void FTimerManager::InternalClearTimer(FTimerHandle const& InHandle)
{
	// not currently threadsafe
	check(IsInGameThread());

	// Skip if the handle is invalid as it  would not be found by FindTimer and unbind the current handler if it also used INDEX_NONE. 
	if (!InHandle.IsValid())
	{
		return;
	}

	int32 TimerIdx;
	FTimerData const* const TimerData = FindTimer(InHandle, &TimerIdx);
	if (TimerData)
	{
		InternalClearTimer(TimerIdx, TimerData->Status);
	}
}

void FTimerManager::InternalClearTimer(int32 TimerIdx, ETimerStatus TimerStatus)
{
	switch (TimerStatus)
	{
		case ETimerStatus::Pending:
			PendingTimerList.RemoveAtSwap(TimerIdx);
			break;

		case ETimerStatus::Active:
			ActiveTimerHeap.HeapRemoveAt(TimerIdx);
			break;

		case ETimerStatus::Paused:
			PausedTimerList.RemoveAtSwap(TimerIdx);
			break;

		case ETimerStatus::Executing:
			// Edge case. We're currently handling this timer when it got cleared.  Clear it to prevent it firing again
			// in case it was scheduled to fire multiple times.
			CurrentlyExecutingTimer.Clear();
			break;

		default:
			check(false);
	}
}


void FTimerManager::InternalClearAllTimers(void const* Object)
{
	if (Object)
	{
		// search active timer heap for timers using this object and remove them
		int32 const OldActiveHeapSize = ActiveTimerHeap.Num();

		for (int32 Idx=0; Idx<ActiveTimerHeap.Num(); ++Idx)
		{
			if ( ActiveTimerHeap[Idx].TimerDelegate.IsBoundToObject(Object) )
			{
				// remove this item
				// this will break the heap property, but we will re-heapify afterward
				ActiveTimerHeap.RemoveAtSwap(Idx--);
			}
		}
		if (OldActiveHeapSize != ActiveTimerHeap.Num())
		{
			// need to heapify
			ActiveTimerHeap.Heapify();
		}

		// search paused timer list for timers using this object and remove them, too
		for (int32 Idx=0; Idx<PausedTimerList.Num(); ++Idx)
		{
			if (PausedTimerList[Idx].TimerDelegate.IsBoundToObject(Object))
			{
				PausedTimerList.RemoveAtSwap(Idx--);
			}
		}

		// search pending timer list for timers using this object and remove them, too
		for (int32 Idx=0; Idx<PendingTimerList.Num(); ++Idx)
		{
			if (PendingTimerList[Idx].TimerDelegate.IsBoundToObject(Object))
			{
				PendingTimerList.RemoveAtSwap(Idx--);
			}
		}

		// Edge case. We're currently handling this timer when it got cleared.  Unbind it to prevent it firing again
		// in case it was scheduled to fire multiple times.
		if (CurrentlyExecutingTimer.TimerDelegate.IsBoundToObject(Object))
		{
			CurrentlyExecutingTimer.Clear();
		}
	}
}



float FTimerManager::InternalGetTimerRemaining(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		if( TimerData->Status != ETimerStatus::Active )
		{
			// ExpireTime is time remaining for paused timers
			return TimerData->ExpireTime;
		}
		else
		{
			return TimerData->ExpireTime - InternalTime;
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerElapsed(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		if( TimerData->Status != ETimerStatus::Active)
		{
			// ExpireTime is time remaining for paused timers
			return (TimerData->Rate - TimerData->ExpireTime);
		}
		else
		{
			return (TimerData->Rate - (TimerData->ExpireTime - InternalTime));
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerRate(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		return TimerData->Rate;
	}
	return -1.f;
}

void FTimerManager::InternalPauseTimer( FTimerData const* TimerToPause, int32 TimerIdx )
{
	// not currently threadsafe
	check(IsInGameThread());

	if( TimerToPause && (TimerToPause->Status != ETimerStatus::Paused) )
	{
		ETimerStatus PreviousStatus = TimerToPause->Status;

		// Don't pause the timer if it's currently executing and isn't going to loop
		if( PreviousStatus != ETimerStatus::Executing || TimerToPause->bLoop )
		{
			// Add to Paused list
			int32 NewIndex = PausedTimerList.Add(*TimerToPause);

			// Set new status
			FTimerData& NewTimer = PausedTimerList[NewIndex];
			NewTimer.Status = ETimerStatus::Paused;

			// Store time remaining in ExpireTime while paused
			NewTimer.ExpireTime = NewTimer.ExpireTime - InternalTime;
		}

		// Remove from previous TArray
		switch( PreviousStatus )
		{
			case ETimerStatus::Active:
				ActiveTimerHeap.HeapRemoveAt(TimerIdx);
				break;

			case ETimerStatus::Pending:
				PendingTimerList.RemoveAtSwap(TimerIdx);
				break;

			case ETimerStatus::Executing:
				CurrentlyExecutingTimer.Clear();
				break;

			default:
				check(false);
		}
	}
}

void FTimerManager::InternalUnPauseTimer(int32 PausedTimerIdx)
{
	// not currently threadsafe
	check(IsInGameThread());

	if (PausedTimerIdx != INDEX_NONE)
	{
		FTimerData& TimerToUnPause = PausedTimerList[PausedTimerIdx];
		check(TimerToUnPause.Status == ETimerStatus::Paused);

		// Move it out of paused list and into proper TArray
		if( HasBeenTickedThisFrame() )
		{
			// Convert from time remaining back to a valid ExpireTime
			TimerToUnPause.ExpireTime += InternalTime;
			TimerToUnPause.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush(TimerToUnPause);
		}
		else
		{
			TimerToUnPause.Status = ETimerStatus::Pending;
			PendingTimerList.Add(TimerToUnPause);
		}

		// remove from paused list
		PausedTimerList.RemoveAtSwap(PausedTimerIdx);
	}
}

// ---------------------------------
// Public members
// ---------------------------------

void FTimerManager::Tick(float DeltaTime)
{
	// @todo, might need to handle long-running case
	// (e.g. every X seconds, renormalize to InternalTime = 0)

	InternalTime += DeltaTime;

	while (ActiveTimerHeap.Num() > 0)
	{
		FTimerData& Top = ActiveTimerHeap.HeapTop();
		if (InternalTime > Top.ExpireTime)
		{
			// Timer has expired! Fire the delegate, then handle potential looping.

			// Remove it from the heap and store it while we're executing
			ActiveTimerHeap.HeapPop(CurrentlyExecutingTimer);
			CurrentlyExecutingTimer.Status = ETimerStatus::Executing;

			// Determine how many times the timer may have elapsed (e.g. for large DeltaTime on a short looping timer)
			int32 const CallCount = CurrentlyExecutingTimer.bLoop ? 
				FMath::TruncToInt( (InternalTime - CurrentlyExecutingTimer.ExpireTime) / CurrentlyExecutingTimer.Rate ) + 1
				: 1;

			// Now call the function
			for (int32 CallIdx=0; CallIdx<CallCount; ++CallIdx)
			{ 
				CurrentlyExecutingTimer.TimerDelegate.Execute();

				// If timer was cleared in the delegate execution, don't execute further 
				if( CurrentlyExecutingTimer.Status != ETimerStatus::Executing )
				{
					break;
				}
			}

			// Status test needed to ensure it didn't get cleared during execution
			if( CurrentlyExecutingTimer.bLoop && CurrentlyExecutingTimer.Status == ETimerStatus::Executing )
			{
				// if timer requires a delegate, make sure it's still validly bound (i.e. the delegate's object didn't get deleted or something)
				if (!CurrentlyExecutingTimer.bRequiresDelegate || CurrentlyExecutingTimer.TimerDelegate.IsBound())
				{
					// Put this timer back on the heap
					CurrentlyExecutingTimer.ExpireTime += CallCount * CurrentlyExecutingTimer.Rate;
					CurrentlyExecutingTimer.Status = ETimerStatus::Active;
					ActiveTimerHeap.HeapPush(CurrentlyExecutingTimer);
				}
			}

			CurrentlyExecutingTimer.Clear();
		}
		else
		{
			// no need to go further down the heap, we can be finished
			break;
		}
	}

	// Timer has been ticked.
	LastTickedFrame = GFrameCounter;

	// If we have any Pending Timers, add them to the Active Queue.
	if( PendingTimerList.Num() > 0 )
	{
		for(int32 Index=0; Index<PendingTimerList.Num(); Index++)
		{
			FTimerData& TimerToActivate = PendingTimerList[Index];
			// Convert from time remaining back to a valid ExpireTime
			TimerToActivate.ExpireTime += InternalTime;
			TimerToActivate.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush( TimerToActivate );
		}
		PendingTimerList.Empty();
	}
}

TStatId FTimerManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTimerManager, STATGROUP_Tickables);
}
