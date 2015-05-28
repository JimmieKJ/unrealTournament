// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TimerManager.h: Global gameplay timer facility
=============================================================================*/

#pragma once

#include "Engine/EngineTypes.h"

DECLARE_DELEGATE(FTimerDelegate);

/** Simple interface to wrap a timer delegate that can be either native or dynamic. */
struct FTimerUnifiedDelegate
{
	/** Holds the delegate to call. */
	FTimerDelegate FuncDelegate;
	/** Holds the dynamic delegate to call. */
	FTimerDynamicDelegate FuncDynDelegate;

	FTimerUnifiedDelegate() {};
	FTimerUnifiedDelegate(FTimerDelegate const& D) : FuncDelegate(D) {};
	FTimerUnifiedDelegate(FTimerDynamicDelegate const& D) : FuncDynDelegate(D) {};
	
	inline void Execute()
	{
		if (FuncDelegate.IsBound())
		{
#if STATS
			TStatId StatId = TStatId();
			UObject* Object = FuncDelegate.GetUObject();
			if (Object)
			{
				StatId = Object->GetStatID();
			}
			FScopeCycleCounter Context(StatId);
#endif
			FuncDelegate.Execute();
		}
		else if (FuncDynDelegate.IsBound())
		{
			FuncDynDelegate.ProcessDelegate<UObject>(nullptr);
		}
	}

	inline bool IsBound() const
	{
		return ( FuncDelegate.IsBound() || FuncDynDelegate.IsBound() );
	}

	inline bool IsBoundToObject(void const* Object) const
	{
		if (FuncDelegate.IsBound())
		{
			return FuncDelegate.IsBoundToObject(Object);
		}
		else if (FuncDynDelegate.IsBound())
		{
			return FuncDynDelegate.IsBoundToObject(Object);
		}

		return false;
	}

	inline void Unbind()
	{
		FuncDelegate.Unbind();
		FuncDynDelegate.Unbind();
	}
};

// Unique handle that can be used to distinguish timers that have identical delegates.
struct FTimerHandle
{
	FTimerHandle()
	: Handle(INDEX_NONE)
	{

	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	void Invalidate()
	{
		Handle = INDEX_NONE;
	}

	void MakeValid();

	bool operator==(const FTimerHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FTimerHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

private:
	int32 Handle;
};

enum class ETimerStatus : uint8
{
	Pending,
	Active,
	Paused,
	Executing
};

struct FTimerData
{
	/** If true, this timer will loop indefinitely.  Otherwise, it will be destroyed when it expires. */
	uint8 bLoop : 1;

	/** If true, this timer was created with a delegate to call (which means if the delegate becomes invalid, we should invalidate the timer too). */
	uint8 bRequiresDelegate : 1;

	/** Timer Status */
	ETimerStatus Status;
	
	/** Time between set and fire, or repeat frequency if looping. */
	float Rate;

	/** 
	 * Time (on the FTimerManager's clock) that this timer should expire and fire its delegate. 
	 * Note when a timer is paused, we re-base ExpireTime to be relative to 0 instead of the running clock, 
	 * meaning ExpireTime contains the remaining time until fire.
	 */
	double ExpireTime;

	/** Holds the delegate to call. */
	FTimerUnifiedDelegate TimerDelegate;

	FTimerHandle TimerHandle;

	FTimerData()
		: bLoop(false)
		, bRequiresDelegate(false)
		, Status(ETimerStatus::Active)
		, Rate(0)
		, ExpireTime(0)
	{}

	/** Operator less, used to sort the heap based on time until execution. **/
	bool operator<(const FTimerData& Other) const
	{
		return ExpireTime < Other.ExpireTime;
	}

	void Clear()
	{
		TimerDelegate.Unbind();
		TimerHandle.Invalidate();
	}
};


/** 
 * Class to globally manage timers.
 */
class ENGINE_API FTimerManager : public FNoncopyable
{
public:

	// ----------------------------------
	// FTickableGameObject interface

	void Tick(float DeltaTime);
	TStatId GetStatId() const;


	// ----------------------------------
	// Timer API

	FTimerManager()
		: InternalTime(0.0)
		, LastTickedFrame(static_cast<uint64>(-1))
	{}


	/**
	 * Sets a timer to call the given native function at a set interval.  If a timer is already set
	 * for this delegate, it will update the current timer to the new parameters and reset its 
	 * elapsed time to 0.
	 *
	 * @param InObj					Object to call the timer function on.
	 * @param InTimerMethod			Method to call when timer fires.
	 * @param InRate				The amount of time between set and firing.  If <= 0.f, clears existing timers.
	 * @param InbLoop				true to keep firing at Rate intervals, false to fire only once.
	 * @param InFirstDelay			The time for the first iteration of a looping timer. If < 0.f inRate will be used.
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of SetTimer is deprecated, please pass a FTimerHandle as the first argument.")
	FORCEINLINE void SetTimer(UserClass* InObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		DEPRECATED_InternalSetTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of SetTimer is deprecated, please pass a FTimerHandle as the first argument.")
	FORCEINLINE void SetTimer(UserClass* InObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		DEPRECATED_InternalSetTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay );
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of SetTimer is deprecated, please pass a FTimerHandle as the first argument.")
	FORCEINLINE void SetTimer(FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		DEPRECATED_InternalSetTimer( FTimerUnifiedDelegate(InDelegate), InRate, InbLoop, InFirstDelay );
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of SetTimer is deprecated, please pass a FTimerHandle as the first argument.")
	FORCEINLINE void SetTimer(FTimerDynamicDelegate const& InDynDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		DEPRECATED_InternalSetTimer( FTimerUnifiedDelegate(InDynDelegate), InRate, InbLoop, InFirstDelay );
	}

	/**
	 * Sets a timer to call the given native function at a set interval.  If a timer is already set
	 * for this delegate, it will update the current timer to the new parameters and reset its
	 * elapsed time to 0.
	 *
	 * @param InOutHandle			Handle to identify this timer. If it is invalid when passed in it will be made into a valid handle.
	 * @param InObj					Object to call the timer function on.
	 * @param InTimerMethod			Method to call when timer fires.
	 * @param InRate					The amount of time between set and firing.  If <= 0.f, clears existing timers.
	 * @param InbLoop				true to keep firing at Rate intervals, false to fire only once.
	 * @param InFirstDelay			The time for the first iteration of a looping timer. If < 0.f inRate will be used.
	 */
	template< class UserClass >
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay);
	}
	template< class UserClass >
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay);
	}

	/** Version that takes any generic delegate. */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDelegate), InRate, InbLoop, InFirstDelay);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDynamicDelegate const& InDynDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDynDelegate), InRate, InbLoop, InFirstDelay);
	}
	/*** Version that doesn't take a delegate */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(), InRate, InbLoop, InFirstDelay);
	}

	/**
	 * Sets a timer to call the given native function on the next tick.
	 *
	 * @param inObj					Object to call the timer function on.
	 * @param inTimerMethod			Method to call when timer fires.
	 */
	template< class UserClass >
	FORCEINLINE void SetTimerForNextTick(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
	}
	template< class UserClass >
	FORCEINLINE void SetTimerForNextTick(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
	}

	/** Version that takes any generic delegate. */
	FORCEINLINE void SetTimerForNextTick(FTimerDelegate const& InDelegate)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(InDelegate));
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	FORCEINLINE void SetTimerForNextTick(FTimerDynamicDelegate const& InDynDelegate)
	{
		InternalSetTimerForNextTick(FTimerUnifiedDelegate(InDynDelegate));
	}


	/**
	 * Clears a previously set timer, identical to calling SetTimer() with a <= 0.f rate.
	 *
	 * @param inObj			Object to call the timer function on.
	 * @param inTimerMethod Method to call when timer fires
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of ClearTimer is deprecated, use ClearTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void ClearTimer( UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod)
	{
		DEPRECATED_InternalClearTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of ClearTimer is deprecated, use ClearTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void ClearTimer( UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod)
	{
		DEPRECATED_InternalClearTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of ClearTimer is deprecated, use ClearTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void ClearTimer(FTimerDelegate const& InDelegate)
	{
		DEPRECATED_InternalClearTimer( FTimerUnifiedDelegate(InDelegate) );
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of ClearTimer is deprecated, use ClearTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void ClearTimer(FTimerDynamicDelegate const& InDynDelegate)
	{
		DEPRECATED_InternalClearTimer( FTimerUnifiedDelegate(InDynDelegate) );
	}
	/** Version that takes a handle */
	FORCEINLINE void ClearTimer(FTimerHandle InHandle)
	{
		InternalClearTimer(InHandle);
	}

	/** Clears all timers that are bound to functions on the given object. */
	FORCEINLINE void ClearAllTimersForObject(void const* Object)
	{
		if (Object)
		{
			InternalClearAllTimers( Object );
		}
	}


	/**
	 * Pauses a previously set timer.
	 *
	 * @param inObj				Object to call the timer function on.
	 * @param inTimerMethod		Method to call when timer fires.
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of PauseTimer is deprecated, use PauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void PauseTimer(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod)
	{
		int32 TimerIdx;
		FTimerData const* TimerToPause = DEPRECATED_FindTimer(FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ), &TimerIdx);
		InternalPauseTimer(TimerToPause, TimerIdx);
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of PauseTimer is deprecated, use PauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void PauseTimer(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod)
	{
		int32 TimerIdx;
		FTimerData const* TimerToPause = DEPRECATED_FindTimer(FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ), &TimerIdx);
		InternalPauseTimer(TimerToPause, TimerIdx);
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of PauseTimer is deprecated, use PauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void PauseTimer(FTimerDelegate const& InDelegate)
	{
		int32 TimerIdx;
		FTimerData const* TimerToPause = DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDelegate), &TimerIdx);
		InternalPauseTimer(TimerToPause, TimerIdx);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of PauseTimer is deprecated, use PauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void PauseTimer(FTimerDynamicDelegate const& InDynDelegate)
	{
		int32 TimerIdx;
		FTimerData const* TimerToPause = DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDynDelegate), &TimerIdx);
		InternalPauseTimer(TimerToPause, TimerIdx);
	}
	/** Version that takes a handle */
	FORCEINLINE void PauseTimer(FTimerHandle InHandle)
	{
		int32 TimerIdx;
		FTimerData const* TimerToPause = FindTimer(InHandle, &TimerIdx);
		InternalPauseTimer(TimerToPause, TimerIdx);
	}

	/**
	 * Unpauses a previously set timer
	 *
	 * @param inObj				Object to call the timer function on.
	 * @param inTimerMethod		Method to call when timer fires.
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of UnPauseTimer is deprecated, use UnPauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void UnPauseTimer(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod)
	{
		int32 TimerIdx = DEPRECATED_FindTimerInList( PausedTimerList, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		InternalUnPauseTimer(TimerIdx);
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of UnPauseTimer is deprecated, use UnPauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void UnPauseTimer(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod)
	{
		int32 TimerIdx = DEPRECATED_FindTimerInList( PausedTimerList, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		InternalUnPauseTimer(TimerIdx);
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of UnPauseTimer is deprecated, use UnPauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void UnPauseTimer(FTimerDelegate const& InDelegate)
	{
		int32 TimerIdx = DEPRECATED_FindTimerInList( PausedTimerList, FTimerUnifiedDelegate(InDelegate) );
		InternalUnPauseTimer(TimerIdx);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of UnPauseTimer is deprecated, use UnPauseTimer(FTimerHandle InHandle) instead.")
	FORCEINLINE void UnPauseTimer(FTimerDynamicDelegate const& InDynDelegate)
	{
		int32 TimerIdx = DEPRECATED_FindTimerInList( PausedTimerList, FTimerUnifiedDelegate(InDynDelegate) );
		InternalUnPauseTimer(TimerIdx);
	}
	/** Version that takes a handle */
	FORCEINLINE void UnPauseTimer(FTimerHandle InHandle)
	{
		int32 TimerIdx = FindTimerInList(PausedTimerList, InHandle);
		InternalUnPauseTimer(TimerIdx);
	}

	/**
	 * Gets the current rate (time between activations) for the specified timer.
	 *
	 * @param inObj			Object to call the timer function on.
	 * @param inTimerMethod Method to call when timer fires.
	 * @return				The current rate or -1.f if timer does not exist.
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of GetTimerRate is deprecated, use GetTimerRate(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRate(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return InternalGetTimerRate(TimerData);
	}

	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of GetTimerRate is deprecated, use GetTimerRate(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRate(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return InternalGetTimerRate(TimerData);
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of GetTimerRate is deprecated, use GetTimerRate(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRate(FTimerDelegate const& InDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDelegate) );
		return InternalGetTimerRate(TimerData);
	}

	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of GetTimerRate is deprecated, use GetTimerRate(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRate(FTimerDynamicDelegate const& InDynDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDynDelegate) );
		return InternalGetTimerRate(TimerData);
	}

	/** Version that takes a handle */
	FORCEINLINE float GetTimerRate(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerRate(TimerData);
	}

	/**
	 * Returns true if the specified timer exists and is not paused
	 *
	 * @param inObj			Object binding for query.
	 * @param inTimerMethod Method binding for query.
	 * @return				true if the timer matching the given criteria exists and is active, false otherwise.
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of IsTimerActive is deprecated, use IsTimerActive(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerActive(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return TimerData && TimerData->Status != ETimerStatus::Paused;
	}
		
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of IsTimerActive is deprecated, use IsTimerActive(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerActive(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return TimerData && TimerData->Status != ETimerStatus::Paused;
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of IsTimerActive is deprecated, use IsTimerActive(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerActive(FTimerDelegate const& InDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDelegate) );
		return TimerData && TimerData->Status != ETimerStatus::Paused;
	}

	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of IsTimerActive is deprecated, use IsTimerActive(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerActive(FTimerDynamicDelegate const& InDynDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDynDelegate) );
		return TimerData && TimerData->Status != ETimerStatus::Paused;
	}

	/** Version that takes a handle */
	FORCEINLINE bool IsTimerActive(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer( InHandle );
		return TimerData && TimerData->Status != ETimerStatus::Paused;
	}

	/**
	* Returns true if the specified timer exists and is paused
	*
	* @param inObj			Object binding for query.
	* @param inTimerMethod	Method binding for query.
	* @return				true if the timer matching the given criteria exists and is paused, false otherwise.
	*/
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of IsTimerPaused is deprecated, use IsTimerPaused(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPaused(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
		return TimerData && TimerData->Status == ETimerStatus::Paused;
	}

	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of IsTimerPaused is deprecated, use IsTimerPaused(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPaused(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod)  const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
		return TimerData && TimerData->Status == ETimerStatus::Paused;
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of IsTimerPaused is deprecated, use IsTimerPaused(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPaused(FTimerDelegate const& InDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDelegate));
		return TimerData && TimerData->Status == ETimerStatus::Paused;
	}

	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of IsTimerPaused is deprecated, use IsTimerPaused(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPaused(FTimerDynamicDelegate const& InDynDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDynDelegate));
		return TimerData && TimerData->Status == ETimerStatus::Paused;
	}

	/** Version that takes a handle */
	FORCEINLINE bool IsTimerPaused(FTimerHandle InHandle)
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return TimerData && TimerData->Status == ETimerStatus::Paused;
	}

	/**
	* Returns true if the specified timer exists and is pending
	*
	* @param inObj			Object binding for query.
	* @param inTimerMethod	Method binding for query.
	* @return				true if the timer matching the given criteria exists and is paused, false otherwise.
	*/
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of IsTimerPending is deprecated, use IsTimerPending(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPending(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
		return TimerData && TimerData->Status == ETimerStatus::Pending;
	}

	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of IsTimerPending is deprecated, use IsTimerPending(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPending(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod)  const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
		return TimerData && TimerData->Status == ETimerStatus::Pending;
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of IsTimerPending is deprecated, use IsTimerPending(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPending(FTimerDelegate const& InDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDelegate));
		return TimerData && TimerData->Status == ETimerStatus::Pending;
	}

	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of IsTimerPending is deprecated, use IsTimerPending(FTimerHandle InHandle) instead.")
	FORCEINLINE bool IsTimerPending(FTimerDynamicDelegate const& InDynDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDynDelegate));
		return TimerData && TimerData->Status == ETimerStatus::Pending;
	}

	/** Version that takes a handle */
	FORCEINLINE bool IsTimerPending(FTimerHandle InHandle)
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return TimerData && TimerData->Status == ETimerStatus::Pending;
	}

	/**
	* Returns true if the specified timer exists
	*
	* @param inObj			Object binding for query.
	* @param inTimerMethod	Method binding for query.
	* @return				true if the timer matching the given criteria exists, false otherwise.
	*/
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of TimerExists is deprecated, use TimerExists(FTimerHandle InHandle) instead.")
	FORCEINLINE bool TimerExists(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod) const
	{
		return DEPRECATED_FindTimer(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod))) != nullptr;
	}

	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of TimerExists is deprecated, use TimerExists(FTimerHandle InHandle) instead.")
	FORCEINLINE bool TimerExists(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod) const
	{
		return DEPRECATED_FindTimer(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod))) != nullptr;
	}

	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of TimerExists is deprecated, use TimerExists(FTimerHandle InHandle) instead.")
	FORCEINLINE bool TimerExists(FTimerDelegate const& InDelegate) const
	{
		return DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDelegate)) != nullptr;
		
	}

	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of TimerExists is deprecated, use TimerExists(FTimerHandle InHandle) instead.")
	FORCEINLINE bool TimerExists(FTimerDynamicDelegate const& InDynDelegate) const
	{
		return DEPRECATED_FindTimer(FTimerUnifiedDelegate(InDynDelegate)) != nullptr;
	}

	/** Version that takes a handle */
	FORCEINLINE bool TimerExists(FTimerHandle InHandle)
	{
		return FindTimer(InHandle) != nullptr;
	}

	/**
	 * Gets the current elapsed time for the specified timer.
	 *
	 * @param inObj			Object to call the timer function on
	 * @param inTimerMethod Method to call when timer fires
	 * @return				The current time elapsed or -1.f if timer does not exist.
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of GetTimerElapsed is deprecated, use GetTimerElapsed(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerElapsed(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return InternalGetTimerElapsed(TimerData);
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of GetTimerElapsed is deprecated, use GetTimerElapsed(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerElapsed(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return InternalGetTimerElapsed(TimerData);
	}
	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of GetTimerElapsed is deprecated, use GetTimerElapsed(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerElapsed(FTimerDelegate const& InDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDelegate) );
		return InternalGetTimerElapsed(TimerData);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of GetTimerElapsed is deprecated, use GetTimerElapsed(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerElapsed(FTimerDynamicDelegate const& InDynDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDynDelegate) );
		return InternalGetTimerElapsed(TimerData);
	}

	/** Version that takes a handle */
	FORCEINLINE float GetTimerElapsed(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerElapsed(TimerData);
	}

	/**
	 * Gets the time remaining before the specified timer is called
	 *
	 * @param inObj			Object to call the timer function on.
	 * @param inTimerMethod Method to call when timer fires.
	 * @return				The current time remaining, or -1.f if timer does not exist
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of GetTimerRemaining is deprecated, use GetTimerRemaining(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRemaining(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return InternalGetTimerRemaining(TimerData);
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("This overload of GetTimerRemaining is deprecated, use GetTimerRemaining(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRemaining(UserClass* inObj, typename FTimerDelegate::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr inTimerMethod) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(inObj, inTimerMethod) ) );
		return InternalGetTimerRemaining(TimerData);
	}
	/** Version that takes any generic delegate. */
	DELEGATE_DEPRECATED("This overload of GetTimerRemaining is deprecated, use GetTimerRemaining(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRemaining(FTimerDelegate const& InDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDelegate) );
		return InternalGetTimerRemaining(TimerData);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	DELEGATE_DEPRECATED("This overload of GetTimerRemaining is deprecated, use GetTimerRemaining(FTimerHandle InHandle) instead.")
	FORCEINLINE float GetTimerRemaining(FTimerDynamicDelegate const& InDynDelegate) const
	{
		FTimerData const* const TimerData = DEPRECATED_FindTimer( FTimerUnifiedDelegate(InDynDelegate) );
		return InternalGetTimerRemaining(TimerData);
	}
	/** Version that takes a handle */
	FORCEINLINE float GetTimerRemaining(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerRemaining(TimerData);
	}

	/** Timer manager has been ticked this frame? */
	bool FORCEINLINE HasBeenTickedThisFrame() const
	{
		return (LastTickedFrame == GFrameCounter);
	}

	/**
	 * Finds a handle to a timer bound to a particular dynamic delegate.
	 * This function is intended to be used only by the K2 system.
	 *
	 * @param  InDynamicDelegate  The dynamic delegate to search for.
	 *
	 * @return A handle to the found timer - !IsValid() if no such timer was found.
	 */
	FTimerHandle K2_FindDynamicTimerHandle(FTimerDynamicDelegate InDynamicDelegate) const;

private:

	void DEPRECATED_InternalSetTimer( FTimerUnifiedDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay );
	void InternalSetTimer( FTimerHandle& InOutHandle, FTimerUnifiedDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay );
	void InternalSetTimer( FTimerData& NewTimerData, float InRate, bool InbLoop, float InFirstDelay );
	void InternalSetTimerForNextTick( FTimerUnifiedDelegate const& InDelegate );
	void DEPRECATED_InternalClearTimer( FTimerUnifiedDelegate const& InDelegate );
	void InternalClearTimer( FTimerHandle const& InDelegate );
	void InternalClearTimer( int32 TimerIdx, ETimerStatus TimerStatus );
	void InternalClearAllTimers( void const* Object );

	/** Will find a timer in the active, paused, or pending list. */
	FTimerData const* DEPRECATED_FindTimer( FTimerUnifiedDelegate const& InDelegate, int32* OutTimerIndex=nullptr ) const;
	FTimerData const* FindTimer( FTimerHandle const& InHandle, int32* OutTimerIndex = nullptr ) const;

	void InternalPauseTimer( FTimerData const* TimerToPause, int32 TimerIdx );
	void InternalUnPauseTimer( int32 PausedTimerIdx );
	
	float InternalGetTimerRate( FTimerData const* const TimerData ) const;
	float InternalGetTimerElapsed( FTimerData const* const TimerData ) const;
	float InternalGetTimerRemaining( FTimerData const* const TimerData ) const;

	/** Will find the given timer in the given TArray and return its index. */ 
	int32 DEPRECATED_FindTimerInList( const TArray<FTimerData> &SearchArray, FTimerUnifiedDelegate const& InDelegate ) const;
	int32 FindTimerInList( const TArray<FTimerData> &SearchArray, FTimerHandle const& InHandle ) const;

	/** Heap of actively running timers. */
	TArray<FTimerData> ActiveTimerHeap;
	/** Unordered list of paused timers. */
	TArray<FTimerData> PausedTimerList;
	/** List of timers added this frame, to be added after timer has been ticked */
	TArray<FTimerData> PendingTimerList;

	/** An internally consistent clock, independent of World.  Advances during ticking. */
	double InternalTime;

	/** Timer delegate currently being executed.  Used to handle "timer delegates that manipulate timers" cases. */
	FTimerData CurrentlyExecutingTimer;

	/** Set this to GFrameCounter when Timer is ticked. To figure out if Timer has been already ticked or not this frame. */
	uint64 LastTickedFrame;
};

