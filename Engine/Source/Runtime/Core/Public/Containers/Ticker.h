// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegate.h"


/**
 * Ticker delegates return true to automatically reschedule at the same delay or false for one-shot.
 * You will not get more than one fire per "frame", which is just a FTicker::Tick call.
 * Argument is DeltaTime
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FTickerDelegate, float);

/**
 * Ticker class. Fires delegates after a delay.
 */
class FTicker
{
public:

	/** Singleton used for the ticker in Core / Launch. If you add a new ticker for a different subsystem, do not put that singleton here! **/
	CORE_API static FTicker& GetCoreTicker();

	FTicker()
		: CurrentTime(0.0)
	{
	}

	/**
	 * Add a new ticker with a given delay / interval
	 * @param InDelegate	delegate to fire after the delay
	 * @param InDelay		Delay until next fire; 0 means "next frame"
	 */
	FDelegateHandle AddTicker(const FTickerDelegate& InDelegate, float InDelay = 0.0f)
	{
		FElement Ticker(CurrentTime + InDelay, InDelay, InDelegate);
		PriorityQueue.HeapPush(Ticker);
		return InDelegate.GetHandle();
	}

	/**
	 * Removes a previously added ticker delegate.
	 *
	 * @param Handle - The handle of the ticker to remove.
	 */
	void RemoveTicker(FDelegateHandle Handle)
	{
		PriorityQueue.RemoveAll([=](const FElement& Element){ return Element.Delegate.GetHandle() == Handle; });
	}

	/**
	 * Removes a previously added ticker delegate.
	 *
	 * @param Delegate - The delegate to remove.
	 */
	DELEGATE_DEPRECATED("This RemoveTicker overload is deprecated - please remove tickers using the FDelegateHandle returned by the AddTicker function.")
	void RemoveTicker(const FTickerDelegate& Delegate)
	{
		for (int32 Index = 0; Index < PriorityQueue.Num(); ++Index)
		{
			if (PriorityQueue[Index].Delegate.DEPRECATED_Compare(Delegate))
			{
				PriorityQueue.RemoveAt(Index);
			}
		}
	}

	/**
	 * Fire all tickers who have passed their delay and reschedule the ones that return true
	 * @param DeltaTime	time that has passed since the last tick call
	 */
	void Tick(float DeltaTime)
	{
		if (!PriorityQueue.Num())
		{
			return;
		}
		double Now = CurrentTime + DeltaTime;
		TArray<FElement> ToFire;
		while (PriorityQueue.Num() && PriorityQueue.HeapTop().FireTime <= Now)
		{
			ToFire.Add(PriorityQueue.HeapTop());
			PriorityQueue.HeapPopDiscard();
		}
		CurrentTime = Now;
		for (int32 Index = 0; Index < ToFire.Num(); Index++)
		{
			FElement& FireIt = ToFire[Index];
			if (FireIt.Fire(DeltaTime))
			{
				FireIt.FireTime = CurrentTime + FireIt.DelayTime;
				PriorityQueue.HeapPush(FireIt);
			}
		}
	}

private:

	/**
	 * Element of the priority queue
	 */
	struct FElement
	{
		/** Time that this delegate must not fire before **/
		double FireTime;
		/** Delay that this delegate was scheduled with. Kept here so that if the delegate returns true, we will reschedule it. **/
		float DelayTime;
		/** Delegate to fire **/
		FTickerDelegate Delegate;

		FElement(double InFireTime, float InDelayTime, const FTickerDelegate& InDelegate)
			: FireTime(InFireTime)
			, DelayTime(InDelayTime)
			, Delegate(InDelegate)
		{
		}
		/** operator less, used to sort the heap based on fire time **/
		bool operator<(const FElement& Other) const
		{
			return FireTime < Other.FireTime;
		}
		/** Fire the delegate if it is fireable **/
		bool Fire(float DeltaTime)
		{
			if (Delegate.IsBound())
			{
				return Delegate.Execute(DeltaTime);
			}
			return false;
		}
	};

	/** Current time of the ticker **/
	double CurrentTime;
	/** Future delegates to fire **/
	TArray<FElement> PriorityQueue;
};

/**
 * Base class for ticker objects
 */
class FTickerObjectBase
{
public:

	/**
	 * Constructor
	 *
	 * @param InDelay Delay until next fire; 0 means "next frame"
	 */
	FTickerObjectBase(float InDelay = 0.0f)
	{
		// Register delegate for ticker callback
		FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FTickerObjectBase::Tick);
		TickHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, InDelay);
	}

	/**
	 * Destructor
	 */
	virtual ~FTickerObjectBase()
	{
		// Unregister ticker delegate
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class.
	 *
	 * @param DeltaTime	time passed since the last call.
	 * @return true if should continue ticking
	 */
	virtual bool Tick(float DeltaTime) = 0;

private:	

	/** Delegate for callbacks to Tick */
	FDelegateHandle TickHandle;
};
