// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/BitArray.h"
#include "NavigationTypes.generated.h"

#define INVALID_NAVNODEREF (0)
#define INVALID_NAVQUERYID uint32(0)
#define INVALID_NAVDATA uint32(0)
#define INVALID_NAVEXTENT (FVector::ZeroVector)

#define DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL 50.f
#define DEFAULT_NAV_QUERY_EXTENT_VERTICAL 250.f

/** uniform identifier type for navigation data elements may it be a polygon or graph node */
typedef uint64 NavNodeRef;

struct FNavigationQueryFilter;
class AActor;
class ANavigationData;
class INavAgentInterface;
class INavRelevantInterface;

namespace FNavigationSystem
{
	/** used as a fallback value for navigation agent radius, when none specified via UNavigationSystem::SupportedAgents */
	extern const float FallbackAgentRadius;

	/** used as a fallback value for navigation agent height, when none specified via UNavigationSystem::SupportedAgents */
	extern const float FallbackAgentHeight;

	static const FBox InvalidBoundingBox(0);

	static const FVector InvalidLocation = FVector(FLT_MAX);

	FORCEINLINE bool IsValidLocation(const FVector& TestLocation)
	{
		return TestLocation != InvalidLocation;
	}

	enum ECreateIfEmpty
	{
		Invalid = -1,
		DontCreate = 0,
		Create = 1,
	};
}

UENUM()
namespace ENavigationOptionFlag
{
	enum Type
	{
		Default,
		Enable UMETA(DisplayName = "Yes"),	// UHT was complaining when tried to use True as value instead of Enable
		Disable UMETA(DisplayName = "No"),

		MAX UMETA(Hidden)
	};
}

//////////////////////////////////////////////////////////////////////////
// Navigation data generation

namespace ENavigationDirtyFlag
{
	enum Type
	{
		Geometry			= (1 << 0),
		DynamicModifier		= (1 << 1),
		UseAgentHeight		= (1 << 2),
		NavigationBounds	= (1 << 3),

		All				= Geometry | DynamicModifier,		// all rebuild steps here without additional flags
	};
}

struct FNavigationDirtyArea
{
	FBox Bounds;
	int32 Flags;
	
	FNavigationDirtyArea() : Flags(0) {}
	FNavigationDirtyArea(const FBox& InBounds, int32 InFlags) : Bounds(InBounds), Flags(InFlags) {}
	FORCEINLINE bool HasFlag(ENavigationDirtyFlag::Type Flag) const { return (Flags & Flag) != 0; }
};

struct FNavigationBounds
{
	uint32						UniqueID;
	FBox						AreaBox;	 
	TWeakObjectPtr<ULevel>		Level;		// The level this bounds belongs to

	bool operator==(const FNavigationBounds& Other) const 
	{ 
		return UniqueID == Other.UniqueID; 
	}

	friend uint32 GetTypeHash(const FNavigationBounds& NavBounds)
	{
		return GetTypeHash(NavBounds.UniqueID);
	}
};

struct FNavigationBoundsUpdateRequest 
{
	FNavigationBounds NavBounds;
	
	enum Type
	{
		Added,
		Removed,
		Updated,
	};

	Type UpdateRequest;
};

struct FNavigationDirtyElement
{
	/** object owning this element */
	FWeakObjectPtr Owner;
	
	/** cached interface pointer */
	INavRelevantInterface* NavInterface;

	/** override for update flags */
	int32 FlagsOverride;
	
	/** flags of already existing entry for this actor */
	int32 PrevFlags;
	
	/** bounds of already existing entry for this actor */
	FBox PrevBounds;

	/** prev flags & bounds data are set */
	uint8 bHasPrevData : 1;

	/** request was invalidated while queued, use prev values to dirty area */
	uint8 bInvalidRequest : 1;

	FNavigationDirtyElement()
		: NavInterface(0), FlagsOverride(0), bHasPrevData(false), bInvalidRequest(false)
	{
	}

	FNavigationDirtyElement(UObject* InOwner)
		: Owner(InOwner), NavInterface(0), FlagsOverride(0), bHasPrevData(false), bInvalidRequest(false)
	{
	}

	FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, int32 InFlagsOverride = 0)
		: Owner(InOwner), NavInterface(InNavInterface),	FlagsOverride(InFlagsOverride), bHasPrevData(false), bInvalidRequest(false)
	{
	}

	bool operator==(const FNavigationDirtyElement& Other) const 
	{ 
		return Owner == Other.Owner; 
	}

	bool operator==(const UObject*& OtherOwner) const 
	{ 
		return (Owner == OtherOwner);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FNavigationDirtyElement& Info)
	{
		return GetTypeHash(Info.Owner);
	}
};

UENUM()
enum class ENavDataGatheringMode : uint8
{
	Default,
	Instant,
	Lazy
};

UENUM()
enum class ENavDataGatheringModeConfig : uint8
{
	Invalid UMETA(Hidden),
	Instant,
	Lazy
};

//
// Used to gather per instance transforms in a specific area
//
DECLARE_DELEGATE_TwoParams(FNavDataPerInstanceTransformDelegate, const FBox&, TArray<FTransform>&);

//////////////////////////////////////////////////////////////////////////
// Path

struct FNavigationPortalEdge
{
	FVector Left;
	FVector Right;
	NavNodeRef ToRef;

	FNavigationPortalEdge() : Left(0.f), Right(0.f)
	{}

	FNavigationPortalEdge(const FVector& InLeft, const FVector& InRight, NavNodeRef InToRef)
		: Left(InLeft), Right(InRight), ToRef(InToRef)
	{}

	FORCEINLINE FVector GetPoint(const int32 Index) const
	{
		check(Index >= 0 && Index < 2);
		return ((FVector*)&Left)[Index];
	}

	FORCEINLINE float GetLength() const { return FVector::Dist(Left, Right); }

	FORCEINLINE FVector GetMiddlePoint() const { return Left + (Right - Left) / 2; }
};

/** Describes a point in navigation data */
struct FNavLocation
{
	/** location relative to path's base */
	FVector Location;

	/** node reference in navigation data */
	NavNodeRef NodeRef;

	FNavLocation() : Location(FVector::ZeroVector), NodeRef(INVALID_NAVNODEREF) {}
	explicit FNavLocation(const FVector& InLocation, NavNodeRef InNodeRef = INVALID_NAVNODEREF) 
		: Location(InLocation), NodeRef(InNodeRef) {}

	/** checks if location has associated navigation node ref */
	FORCEINLINE bool HasNodeRef() const { return NodeRef != INVALID_NAVNODEREF; }
};

/** Describes node in navigation path */
struct FNavPathPoint : public FNavLocation
{
	/** extra node flags */
	uint32 Flags;

	/** unique Id of custom navigation link starting at this point */
	uint32 CustomLinkId;

	FNavPathPoint() : Flags(0), CustomLinkId(0) {}
	FNavPathPoint(const FVector& InLocation, NavNodeRef InNodeRef = INVALID_NAVNODEREF, uint32 InFlags = 0) 
		: FNavLocation(InLocation, InNodeRef), Flags(InFlags), CustomLinkId(0) {}
};

/** path type data */
struct ENGINE_API FNavPathType
{
	explicit FNavPathType(const FNavPathType* Parent = nullptr) : Id(++NextUniqueId), ParentType(Parent) {}
	FNavPathType(const FNavPathType& Src) : Id(Src.Id), ParentType(Src.ParentType) {}
	
	bool operator==(const FNavPathType& Other) const
	{
		return Id == Other.Id || (ParentType != nullptr && *ParentType == Other);
	}

private:
	static uint32 NextUniqueId;
	uint32 Id;
	const FNavPathType* ParentType;
};

UENUM()
namespace ENavPathEvent
{
	enum Type
	{
		Cleared,
		NewPath,
		UpdatedDueToGoalMoved,
		UpdatedDueToNavigationChanged,
		Invalidated,
		RePathFailed,
		Custom,
	};
}

namespace ENavPathUpdateType
{
	enum Type
	{
		GoalMoved,
		NavigationChanged,
		Custom,
	};
}

namespace EPathObservationResult
{
	enum Type
	{
		NoLongerObserving,
		NoChange,
		RequestRepath,
	};
}

namespace ENavAreaEvent
{
	enum Type
	{
		Registered,
		Unregistered
	};
}

typedef TSharedRef<struct FNavigationPath, ESPMode::ThreadSafe> FNavPathSharedRef;
typedef TSharedPtr<struct FNavigationPath, ESPMode::ThreadSafe> FNavPathSharedPtr;
typedef TWeakPtr<struct FNavigationPath, ESPMode::ThreadSafe> FNavPathWeakPtr;

/** Movement capabilities, determining available movement options for Pawns and used by AI for reachability tests. */
USTRUCT()
struct FMovementProperties
{
	GENERATED_USTRUCT_BODY()

	/** If true, this Pawn is capable of crouching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties)
	uint32 bCanCrouch:1;

	/** If true, this Pawn is capable of jumping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties)
	uint32 bCanJump:1;

	/** If true, this Pawn is capable of walking or moving on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties)
	uint32 bCanWalk:1;

	/** If true, this Pawn is capable of swimming or moving through fluid volumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties)
	uint32 bCanSwim:1;

	/** If true, this Pawn is capable of flying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties)
	uint32 bCanFly:1;

	FMovementProperties()
		: bCanCrouch(false)
		, bCanJump(false)
		, bCanWalk(false)
		, bCanSwim(false)
		, bCanFly(false)
	{
	}
};

/** Properties of representation of an 'agent' (or Pawn) used by AI navigation/pathfinding. */
USTRUCT()
struct ENGINE_API FNavAgentProperties : public FMovementProperties
{
	GENERATED_USTRUCT_BODY()

	/** Radius of the capsule used for navigation/pathfinding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties, meta=(DisplayName="Nav Agent Radius"))
	float AgentRadius;

	/** Total height of the capsule used for navigation/pathfinding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties, meta=(DisplayName="Nav Agent Height"))
	float AgentHeight;

	/** Step height to use, or -1 for default value from navdata's config. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties, meta=(DisplayName="Nav Agent Step Height"))
	float AgentStepHeight;

	/** Scale factor to apply to height of bounds when searching for navmesh to project to when nav walking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementProperties)
	float NavWalkingSearchHeightScale;

	FNavAgentProperties(float Radius = -1.f, float Height = -1.f)
		: AgentRadius(Radius), AgentHeight(Height), AgentStepHeight(-1), NavWalkingSearchHeightScale(0.5f)
	{
	}

	void UpdateWithCollisionComponent(class UShapeComponent* CollisionComponent);

	FORCEINLINE bool IsValid() const { return AgentRadius >= 0.f && AgentHeight >= 0.f; }
	FORCEINLINE bool HasStepHeightOverride() const { return AgentStepHeight >= 0.0f; }

	FORCEINLINE bool IsEquivalent(const FNavAgentProperties& Other, float Precision = 5.f) const
	{
		return FGenericPlatformMath::Abs(AgentRadius - Other.AgentRadius) < Precision &&
			FGenericPlatformMath::Abs(AgentHeight - Other.AgentHeight) < Precision &&
			FGenericPlatformMath::Abs(AgentStepHeight - Other.AgentStepHeight) < Precision;
	}

	bool operator==(const FNavAgentProperties& Other) const
	{
		return IsEquivalent(Other);
	}

	FVector GetExtent() const
	{
		return FVector(AgentRadius, AgentRadius, AgentHeight / 2);
	}

	static const FNavAgentProperties DefaultProperties;
};

inline uint32 GetTypeHash(const FNavAgentProperties& A)
{
	return ((int16(A.AgentRadius) << 16) | int16(A.AgentHeight)) ^ int32(A.AgentStepHeight);
}

USTRUCT()
struct ENGINE_API FNavDataConfig : public FNavAgentProperties
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Display)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Display)
	FColor Color;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Querying)
	FVector DefaultQueryExtent;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadOnly, Category = Navigation)
	TSubclassOf<ANavigationData> NavigationDataClass;

	UPROPERTY(config)
	FStringClassReference NavigationDataClassName;

	FNavDataConfig(float Radius = FNavigationSystem::FallbackAgentRadius, float Height = FNavigationSystem::FallbackAgentHeight);
};

struct FNavigationProjectionWork
{
	const FVector Point;
	FNavLocation OutLocation;
	bool bResult;

	explicit FNavigationProjectionWork(const FVector& StartPoint)
		: Point(StartPoint), bResult(false)
	{}
};

struct FNavigationRaycastWork
{
	const FVector RayStart;
	const FVector RayEnd;
	/** depending on bDidHit HitLocation contains either actual hit location or RayEnd*/
	FNavLocation HitLocation;
	bool bDidHit;

	explicit FNavigationRaycastWork(const FVector& InRayStart, const FVector& InRayEnd)
		: RayStart(InRayStart), RayEnd(InRayEnd), HitLocation(InRayEnd), bDidHit(false)
	{}
};

UENUM()
namespace ENavigationQueryResult
{
	enum Type
	{
		Invalid,
		Error,
		Fail,
		Success
	};
}

struct ENGINE_API FPathFindingQuery
{
	TWeakObjectPtr<const ANavigationData> NavData;
	TWeakObjectPtr<const UObject> Owner;
	FVector StartLocation;
	FVector EndLocation;
	TSharedPtr<const FNavigationQueryFilter> QueryFilter;
	FNavPathSharedPtr PathInstanceToFill;

	/** additional flags passed to navigation data handling request */
	int32 NavDataFlags;

	/** if set, allow partial paths as a result */
	uint32 bAllowPartialPaths : 1;

	FPathFindingQuery() {}

	FPathFindingQuery(const FPathFindingQuery& Source);

	DEPRECATED(4.8, "This version of FPathFindingQuery's constructor is deprecated. Please use ANavigationData reference rather than a pointer")
	FPathFindingQuery(const UObject* InOwner, const ANavigationData* InNavData, const FVector& Start, const FVector& End, TSharedPtr<const FNavigationQueryFilter> SourceQueryFilter = NULL, FNavPathSharedPtr InPathInstanceToFill = NULL);

	FPathFindingQuery(const UObject* InOwner, const ANavigationData& InNavData, const FVector& Start, const FVector& End, TSharedPtr<const FNavigationQueryFilter> SourceQueryFilter = NULL, FNavPathSharedPtr InPathInstanceToFill = NULL);

	explicit FPathFindingQuery(FNavPathSharedRef PathToRecalculate, const ANavigationData* NavDataOverride = NULL);

	FPathFindingQuery& SetPathInstanceToUpdate(FNavPathSharedPtr InPathInstanceToFill) { PathInstanceToFill = InPathInstanceToFill; return *this; }
	FPathFindingQuery& SetAllowPartialPaths(bool bAllow) { bAllowPartialPaths = bAllow; return *this; }
};

namespace EPathFindingMode
{
	enum Type
	{
		Regular,
		Hierarchical,
	};
};

/**
*	Delegate used to communicate that path finding query has been finished.
*	@param uint32 unique Query ID of given query
*	@param ENavigationQueryResult enum expressed query result.
*	@param FNavPathSharedPtr resulting path. Valid only for ENavigationQueryResult == ENavigationQueryResult::Fail
*		(may contain path leading as close to destination as possible)
*		and ENavigationQueryResult == ENavigationQueryResult::Success
*/
DECLARE_DELEGATE_ThreeParams(FNavPathQueryDelegate, uint32, ENavigationQueryResult::Type, FNavPathSharedPtr);

//////////////////////////////////////////////////////////////////////////
// Custom path following data

/** Custom data passed to movement requests. */
struct ENGINE_API FMoveRequestCustomData
{
};

typedef TSharedPtr<FMoveRequestCustomData, ESPMode::ThreadSafe> FCustomMoveSharedPtr;
typedef TWeakPtr<FMoveRequestCustomData, ESPMode::ThreadSafe> FCustomMoveWeakPtr;

UCLASS(Abstract, CustomConstructor)
class UNavigationTypes : public UObject
{
	GENERATED_UCLASS_BODY()

	UNavigationTypes(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { }
};

//////////////////////////////////////////////////////////////////////////
// Memory stating

namespace NavMeshMemory
{
#if STATS
	// @todo could be made a more generic solution
	class FNavigationMemoryStat : public FDefaultAllocator
	{
	public:
		typedef FDefaultAllocator Super;

		class ForAnyElementType : public FDefaultAllocator::ForAnyElementType
		{
		public:
			typedef FDefaultAllocator::ForAnyElementType Super;
		private:
			int32 AllocatedSize;
		public:

			ForAnyElementType()
				: AllocatedSize(0)
			{

			}

			/** Destructor. */
			~ForAnyElementType()
			{
				if (AllocatedSize)
				{
					DEC_DWORD_STAT_BY(STAT_NavigationMemory, AllocatedSize);
				}
			}

			void ResizeAllocation(int32 PreviousNumElements, int32 NumElements, int32 NumBytesPerElement)
			{
				const int32 NewSize = NumElements * NumBytesPerElement;
				INC_DWORD_STAT_BY(STAT_NavigationMemory, NewSize - AllocatedSize);
				AllocatedSize = NewSize;

				Super::ResizeAllocation(PreviousNumElements, NumElements, NumBytesPerElement);
			}

		private:
			ForAnyElementType(const ForAnyElementType&);
			ForAnyElementType& operator=(const ForAnyElementType&);
		};
	};

	typedef FNavigationMemoryStat FNavAllocator;
#else
	typedef FDefaultAllocator FNavAllocator;
#endif
}

#if STATS

template <>
struct TAllocatorTraits<NavMeshMemory::FNavigationMemoryStat> : TAllocatorTraits<NavMeshMemory::FNavigationMemoryStat::Super>
{
};

#endif

template<typename InElementType>
class TNavStatArray : public TArray<InElementType, NavMeshMemory::FNavAllocator>
{
public:
	typedef TArray<InElementType, NavMeshMemory::FNavAllocator> Super;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TNavStatArray() = default;
	TNavStatArray(TNavStatArray&&) = default;
	TNavStatArray(const TNavStatArray&) = default;
	TNavStatArray& operator=(TNavStatArray&&) = default;
	TNavStatArray& operator=(const TNavStatArray&) = default;

#else

	FORCEINLINE TNavStatArray()
	{
	}

	FORCEINLINE TNavStatArray(const TNavStatArray& Other)
		: Super((const Super&)Other)
	{
	}

	FORCEINLINE TNavStatArray(TNavStatArray&& Other)
		: Super((Super&&)Other)
	{
	}

	FORCEINLINE TNavStatArray& operator=(TNavStatArray&& Other)
	{
		(Super&)*this = (Super&&)Other;
		return *this;
	}

	FORCEINLINE TNavStatArray& operator=(const TNavStatArray& Other)
	{
		(Super&)*this = (const Super&)Other;
		return *this;
	}

#endif
};

template<typename InElementType>
struct TContainerTraits<TNavStatArray<InElementType> > : public TContainerTraitsBase<TNavStatArray<InElementType> >
{
	enum { MoveWillEmptyContainer = TContainerTraits<typename TNavStatArray<InElementType>::Super>::MoveWillEmptyContainer };
};

//----------------------------------------------------------------------//
// Active tiles 
//----------------------------------------------------------------------//
struct FNavigationInvokerRaw
{
	FVector Location;
	float RadiusMin;
	float RadiusMax;

	FNavigationInvokerRaw(const FVector& InLocation, float Min, float Max)
		: Location(InLocation), RadiusMin(Min), RadiusMax(Max)
	{}
};

struct FNavigationInvoker
{
	TWeakObjectPtr<AActor> Actor;

	/** tiles GenerationRadius away or close will be generated if they're not already present */
	float GenerationRadius;
	/** tiles over RemovalRadius will get removed.
	*	@Note needs to be >= GenerationRadius or will get clampped */
	float RemovalRadius;

	FNavigationInvoker();
	FNavigationInvoker(AActor& InActor, float InGenerationRadius, float InRemovalRadius);
};

//----------------------------------------------------------------------//
// generic "landscape" support
//----------------------------------------------------------------------//
struct ENGINE_API FNavHeightfieldSamples
{
	TNavStatArray<int16> Heights;
	TBitArray<> Holes;

	FNavHeightfieldSamples();

	FORCEINLINE bool IsEmpty() const { return Heights.Num() == 0; }
};