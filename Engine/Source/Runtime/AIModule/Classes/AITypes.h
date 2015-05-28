// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavigationTypes.h"
#include "AITypes.generated.h"

#define TEXT_AI_LOCATION(v) (FAISystem::IsValidLocation(v) ? *(v).ToString() : TEXT("Invalid"))

namespace FAISystem
{
	static const FRotator InvalidRotation = FRotator(FLT_MAX);
	static const FVector InvalidLocation = FVector(FLT_MAX);
	static const FVector InvalidDirection = FVector::ZeroVector; 
	static const float InvalidRange = -1.f;
	static const float InfiniteInterval = -FLT_MAX;

	FORCEINLINE bool IsValidLocation(const FVector& TestLocation)
	{
		return -InvalidLocation.X < TestLocation.X && TestLocation.X < InvalidLocation.X
			&& -InvalidLocation.Y < TestLocation.Y && TestLocation.Y < InvalidLocation.Y
			&& -InvalidLocation.Z < TestLocation.Z && TestLocation.Z < InvalidLocation.Z;
	}

	FORCEINLINE bool IsValidDirection(const FVector& TestVector)
	{
		return IsValidLocation(TestVector) == true && TestVector.IsZero() == false;
	}

	FORCEINLINE bool IsValidRotation(const FRotator& TestRotation)
	{
		return TestRotation != InvalidRotation;
	}
}

UENUM()
namespace EAIOptionFlag
{
	enum Type
	{
		Default,
		Enable UMETA(DisplayName = "Yes"),	// UHT was complaining when tried to use True as value instead of Enable
		Disable UMETA(DisplayName = "No"),

		MAX UMETA(Hidden)
	};
}

namespace EAIForceParam
{
	enum Type
	{
		Force,
		DoNotForce,

		MAX UMETA(Hidden)
	};
}

namespace FAIMoveFlag
{
	static const bool StopOnOverlap = true;
	static const bool UsePathfinding = true;
	static const bool IgnorePathfinding = false;
}

namespace EAILogicResuming
{
	enum Type
	{
		Continue,
		RestartedInstead,
	};
}

UENUM()
namespace EPawnActionAbortState
{
	enum Type
	{
		NeverStarted,
		NotBeingAborted,
		MarkPendingAbort,	// this means waiting for child to abort before aborting self
		LatentAbortInProgress,
		AbortDone,

		MAX UMETA(Hidden)
	};
}

UENUM()
namespace EPawnActionResult
{
	enum Type
	{
		NotStarted,
		InProgress,
		Success,
		Failed,
		Aborted
	};
}

UENUM()
namespace EPawnActionEventType
{
	enum Type
	{
		Invalid,
		FailedToStart,
		InstantAbort,
		FinishedAborting,
		FinishedExecution,
		Push,
	};
}

UENUM()
namespace EAIRequestPriority
{
	enum Type
	{
		SoftScript, // actions requested by Level Designers by placing AI-hinting elements on the map
		Logic,	// actions AI wants to do due to its internal logic				
		HardScript, // actions LDs really want AI to perform
		Reaction,	// actions being result of game-world mechanics, like hit reactions, death, falling, etc. In general things not depending on what AI's thinking
		Ultimate,	// ultimate priority, to be used with caution, makes AI perform given action regardless of anything else (for example disabled reactions)

		MAX UMETA(Hidden)
	};
}

namespace EAIRequestPriority
{
	static const int32 Lowest = EAIRequestPriority::Logic;
};

UENUM()
namespace EAILockSource
{
	enum Type
	{
		Animation,
		Logic,
		Script,
		Gameplay,

		MAX UMETA(Hidden)
	};
}

/**
 *	TCounter needs to supply following functions:
 *		default constructor
 *		typedef X Type; where X is an integer type to be used as ID's internal type
 *		TCounter::Type GetNextAvailableID() - returns next available ID and advances the internal counter
 *		uint32 GetSize() const - returns number of unique IDs created so far
 *		OnIndexForced(TCounter::Type Index) - called when given Index has been force-used. Counter may need to update "next available ID"
 */

template<typename TCounter>
struct FAINamedID
{
	const typename TCounter::Type Index;
	const FName Name;
protected:
	static TCounter& GetCounter() 
	{ 
		static TCounter Counter;
		return Counter;
	}

	// back-door for forcing IDs
	FAINamedID(const FName& InName, typename TCounter::Type InIndex)
		: Index(InIndex), Name(InName)
	{
		GetCounter().OnIndexForced(InIndex);
	}

public:
	FAINamedID(const FName& InName)
		: Index(GetCounter().GetNextAvailableID()), Name(InName)
	{}

	FAINamedID(const FAINamedID& Other)
		: Index(Other.Index), Name(Other.Name)
	{}

	FAINamedID& operator=(const FAINamedID& Other)
	{
		new(this) FAINamedID(Other);
		return *this;
	}

	FAINamedID()
		: Index(typename TCounter::Type(-1)), Name(TEXT("Invalid"))
	{}

	operator typename TCounter::Type() const { return Index; }
	bool IsValid() const { return Index != InvalidID().Index; }

	static uint32 GetSize() { return GetCounter().GetSize(); }

	static FAINamedID<TCounter> InvalidID()
	{
		static const FAINamedID<TCounter> InvalidIDInstance;
		return InvalidIDInstance;
	}
};

template<typename TCounter>
struct FAIGenericID
{
	const typename TCounter::Type Index;
protected:
protected:
	static TCounter& GetCounter()
	{
		static TCounter Counter;
		return Counter;
	}

	FAIGenericID(typename TCounter::Type InIndex)
		: Index(InIndex)
	{}

public:
	FAIGenericID(const FAIGenericID& Other)
		: Index(Other.Index)
	{}

	FAIGenericID& operator=(const FAIGenericID& Other)
	{
		new(this) FAIGenericID(Other);
		return *this;
	}

	FAIGenericID()
		: Index(typename TCounter::Type(-1))
	{}

	static FAIGenericID GetNextID() { return FAIGenericID(GetCounter().GetNextAvailableID()); }

	operator typename TCounter::Type() const { return Index; }
	bool IsValid() const { return Index != InvalidID().Index; }

	static uint32 GetSize() { return GetCounter().GetSize(); }

	static FAIGenericID<TCounter> InvalidID()
	{
		static const FAIGenericID<TCounter> InvalidIDInstance;
		return InvalidIDInstance;
	}
};

template<typename TCounterType>
struct FAIBasicCounter
{
	typedef TCounterType Type;
protected:
	Type NextAvailableID;
public:
	FAIBasicCounter() : NextAvailableID(Type(0)) {}
	Type GetNextAvailableID() { return NextAvailableID++; }
	uint32 GetSize() const { return uint32(NextAvailableID); }
	void OnIndexForced(Type ForcedIndex) { NextAvailableID = FMath::Max<Type>(ForcedIndex + 1, NextAvailableID); }
};

//////////////////////////////////////////////////////////////////////////
struct AIMODULE_API FAIResCounter : FAIBasicCounter<uint8>
{};
typedef FAINamedID<FAIResCounter> FAIResourceID;

//////////////////////////////////////////////////////////////////////////
struct AIMODULE_API FAIResourcesSet
{
	static const uint32 NoResources = 0;
	static const uint32 AllResources = uint32(-1);
	static const uint8 MaxFlags = 32;
private:
	uint32 Flags;
public:
	FAIResourcesSet(uint32 ResourceSetDescription = NoResources) : Flags(ResourceSetDescription) {}
	FAIResourcesSet(const FAIResourceID& Resource) : Flags(0) 
	{
		AddResource(Resource);
	}

	FAIResourcesSet& AddResourceIndex(uint8 ResourceIndex) { Flags |= (1 << ResourceIndex); return *this; }
	FAIResourcesSet& RemoveResourceIndex(uint8 ResourceIndex) { Flags &= ~(1 << ResourceIndex); return *this; }
	bool ContainsResourceIndex(uint8 ResourceID) const { return (Flags & (1 << ResourceID)) != 0; }

	FAIResourcesSet& AddResource(const FAIResourceID& Resource) { AddResourceIndex(Resource.Index); return *this; }
	FAIResourcesSet& RemoveResource(const FAIResourceID& Resource) { RemoveResourceIndex(Resource.Index); return *this; }
	bool ContainsResource(const FAIResourceID& Resource) const { return ContainsResourceIndex(Resource.Index); }

	bool IsEmpty() const { return Flags == 0; }
	void Clear() { Flags = 0; }
};

/** structure used to define which subsystem requested locking of a specific AI resource (like movement, logic, etc.) */
struct AIMODULE_API FAIResourceLock
{
	/** @note feel free to change the type if you need to support more then 16 lock sources */
	typedef uint16 FLockFlags;

	FLockFlags Locks;
	
	FAIResourceLock();

	FORCEINLINE void SetLock(EAIRequestPriority::Type LockPriority)
	{
		Locks |= (1 << LockPriority);
	}

	FORCEINLINE void ClearLock(EAIRequestPriority::Type LockPriority)
	{
		Locks &= ~(1 << LockPriority);
	}
	
	/** force-clears all locks */
	void ForceClearAllLocks();

	FORCEINLINE bool IsLocked() const
	{
		return Locks != 0;
	}

	FORCEINLINE bool IsLockedBy(EAIRequestPriority::Type LockPriority) const
	{
		return (Locks & (1 << LockPriority)) != 0;
	}

	/** Answers the question if given priority is allowed to use this resource.
	 *	@Note that if resource is locked with priority LockPriority this function will
	 *	return false as well */
	FORCEINLINE bool IsAvailableFor(EAIRequestPriority::Type LockPriority) const
	{
		for (int32 Priority = EAIRequestPriority::MAX - 1; Priority >= LockPriority; --Priority)
		{
			if ((Locks & (1 << Priority)) != 0)
			{
				return false;
			}
		}
		return true;
	}

	FString GetLockPriorityName() const;

	void operator+=(const FAIResourceLock& Other)
	{
		Locks |= Other.Locks;		
	}

	bool operator==(const FAIResourceLock& Other)
	{
		return Locks == Other.Locks;
	}
};

namespace FAIResources
{
	extern AIMODULE_API const FAIResourceID InvalidResource;
	extern AIMODULE_API const FAIResourceID Movement;
	extern AIMODULE_API const FAIResourceID Logic;
	extern AIMODULE_API const FAIResourceID Perception;
	
	AIMODULE_API void RegisterResource(const FAIResourceID& Resource);
	AIMODULE_API const FAIResourceID& GetResource(int32 ResourceIndex);
	AIMODULE_API int32 GetResourcesCount();
	AIMODULE_API FString GetSetDescription(FAIResourcesSet ResourceSet);
}


USTRUCT()
struct AIMODULE_API FAIRequestID
{
	GENERATED_USTRUCT_BODY()
		
private:
	static const uint32 AnyRequestID = 0;
	static const uint32 InvalidRequestID = uint32(-1);

	UPROPERTY()
	uint32 RequestID;

public:
	FAIRequestID(uint32 InRequestID = InvalidRequestID) : RequestID(InRequestID)
	{}

	/** returns true if given ID is identical to stored ID or any of considered
	 *	IDs is FAIRequestID::AnyRequest*/
	FORCEINLINE bool IsEquivalent(uint32 OtherID) const 
	{
		return OtherID != InvalidRequestID && this->IsValid() && (RequestID == OtherID || RequestID == AnyRequestID || OtherID == AnyRequestID);
	}

	FORCEINLINE bool IsEquivalent(FAIRequestID Other) const
	{
		return IsEquivalent(Other.RequestID);
	}

	FORCEINLINE bool IsValid() const
	{
		return RequestID != InvalidRequestID;
	}

	FORCEINLINE uint32 GetID() const { return RequestID; }

	void operator=(uint32 OtherID)
	{
		RequestID = OtherID;
	}

	operator uint32() const
	{
		return RequestID;
	}

	static const FAIRequestID AnyRequest;
	static const FAIRequestID CurrentRequest;
	static const FAIRequestID InvalidRequest;
};

//////////////////////////////////////////////////////////////////////////

class UNavigationQueryFilter;

USTRUCT()
struct AIMODULE_API FAIMoveRequest
{
	GENERATED_USTRUCT_BODY()

	FAIMoveRequest();
	FAIMoveRequest(const AActor* InGoalActor);
	FAIMoveRequest(const FVector& InGoalLocation);

	FAIMoveRequest& SetNavigationFilter(TSubclassOf<UNavigationQueryFilter> Filter) { FilterClass = Filter; return *this; }
	FAIMoveRequest& SetUsePathfinding(bool bPathfinding) { bUsePathfinding = bPathfinding; return *this; }
	FAIMoveRequest& SetAllowPartialPath(bool bAllowPartial) { bAllowPartialPath = bAllowPartial; return *this; }
	FAIMoveRequest& SetProjectGoalLocation(bool bProject) { bProjectGoalOnNavigation = bProject; return *this; }

	FAIMoveRequest& SetCanStrafe(bool bStrafe) { bCanStrafe = bStrafe; return *this; }
	FAIMoveRequest& SetStopOnOverlap(bool bStop) { bStopOnOverlap = bStop; return *this; }
	FAIMoveRequest& SetAcceptanceRadius(float Radius) { AcceptanceRadius = Radius; return *this; }
	FAIMoveRequest& SetUserData(FCustomMoveSharedPtr Data) { UserData = Data; return *this; }

	bool HasGoalActor() const { return bHasGoalActor; }
	AActor* GetGoalActor() const { return bHasGoalActor ? GoalActor : nullptr; }
	FVector GetGoalLocation() const { return GoalLocation; }

	bool IsUsingPathfinding() const { return bUsePathfinding; }
	bool IsUsingPartialPaths() const { return bAllowPartialPath; }
	bool IsProjectingGoal() const { return bProjectGoalOnNavigation; }
	TSubclassOf<UNavigationQueryFilter> GetNavigationFilter() const { return FilterClass; }

	bool CanStrafe() const { return bCanStrafe; }
	bool CanStopOnOverlap() const { return bStopOnOverlap; }
	float GetAcceptanceRadius() const { return AcceptanceRadius; }
	FCustomMoveSharedPtr GetUserData() const { return UserData; }

	void SetGoalActor(const AActor* InGoalActor);
	void SetGoalLocation(const FVector& InGoalLocation);

	bool UpdateGoalLocation(const FVector& NewLocation) const;
	FString ToString() const;

protected:

	/** move goal: actor */
	UPROPERTY()
	AActor* GoalActor;

	/** move goal: location */
	mutable FVector GoalLocation;

	/** pathfinding: navigation filter to use */
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	/** move goal is an actor */
	uint32 bInitialized : 1;

	/** move goal is an actor */
	uint32 bHasGoalActor : 1;

	/** pathfinding: if set - regular pathfinding will be used, if not - direct path between two points */
	uint32 bUsePathfinding : 1;

	/** pathfinding: allow using incomplete path going toward goal but not reaching it */
	uint32 bAllowPartialPath : 1;

	/** pathfinding: goal location will be projected on navigation data before use */
	uint32 bProjectGoalOnNavigation : 1;

	/** pathfollowing: stop move when agent touches/overlaps with goal */
	uint32 bStopOnOverlap : 1;

	/** pathfollowing: keep focal point at move goal */
	uint32 bCanStrafe : 1;

	/** pathfollowing: required distance to goal to complete move */
	float AcceptanceRadius;

	/** pathfollowing: custom user data */
	FCustomMoveSharedPtr UserData;
};
