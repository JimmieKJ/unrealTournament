// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "DataProviders/AIDataProvider.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "EnvQueryTypes.generated.h"

class AActor;
class ARecastNavMesh;
class UNavigationQueryFilter;
class UEnvQueryTest;
class UEnvQueryGenerator;
class UEnvQueryItemType_VectorBase;
class UEnvQueryItemType_ActorBase;
class UEnvQueryContext;
class UEnvQuery;
class UBlackboardData;
class UBlackboardComponent;
struct FEnvQueryInstance;
struct FEnvQueryOptionInstance;
struct FEnvQueryItemDetails;

AIMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogEQS, Warning, All);

// If set, execution details will be processed by debugger
#define USE_EQS_DEBUGGER				(1 && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

DECLARE_STATS_GROUP(TEXT("Environment Query"), STATGROUP_AI_EQS, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick"),STAT_AI_EQS_Tick,STATGROUP_AI_EQS, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick - EQS work"), STAT_AI_EQS_TickWork, STATGROUP_AI_EQS, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick - OnFinished delegates"), STAT_AI_EQS_TickNotifies, STATGROUP_AI_EQS, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick - Removal of completed queries"), STAT_AI_EQS_TickQueryRemovals, STATGROUP_AI_EQS, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Load Time"),STAT_AI_EQS_LoadTime,STATGROUP_AI_EQS, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Execute One Step Time"),STAT_AI_EQS_ExecuteOneStep,STATGROUP_AI_EQS, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Generator Time"),STAT_AI_EQS_GeneratorTime,STATGROUP_AI_EQS, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Test Time"),STAT_AI_EQS_TestTime,STATGROUP_AI_EQS, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Instances"),STAT_AI_EQS_NumInstances,STATGROUP_AI_EQS, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Items"),STAT_AI_EQS_NumItems,STATGROUP_AI_EQS, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Instance memory"),STAT_AI_EQS_InstanceMemory,STATGROUP_AI_EQS, AIMODULE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Avg Instance Response Time (ms)"), STAT_AI_EQS_AvgInstanceResponseTime, STATGROUP_AI_EQS, );

UENUM()
namespace EEnvTestPurpose
{
	enum Type
	{
		Filter UMETA(DisplayName="Filter Only"),
		Score UMETA(DisplayName="Score Only"),
		FilterAndScore UMETA(DisplayName="Filter and Score")
	};
}

UENUM()
namespace EEnvTestFilterType
{
	enum Type
	{
		/** For numeric tests. */
		Minimum,
		/** For numeric tests. */
		Maximum,
		/** For numeric tests. */
		Range,
		/** For Boolean tests. */
		Match
	};
}

UENUM()
namespace EEnvTestScoreEquation
{
	enum Type
	{
		Linear,
		Square,
		InverseLinear,
		SquareRoot,

		Constant
		// What other curve shapes should be supported?  At first I was thinking we'd have parametric (F*V^P + C), but
		// many versions of that curve would violate the [0, 1] output range which I think we should preserve.  So instead
		// I think we should define these by "curve shape".  I'm not sure if we need to allow full tweaks to the curves,
		// such as supporting other "Exponential" curves (positive even powers).  However, I think it's likely that we'll
		// want to support "smooth LERP" / S-shaped curve of the form 2x^3 - 3x^2, and possibly a "sideways" version of
		// the same S-curve.  We also might want to allow "Sine" curves, basically adjusted to match the range and then
		// simply offset by some amount to allow a peak or valley in the middle or on the ends.  (Four Sine options are
		// probably sufficient.)  I'm not sure if Sine is really needed though, so probably we should only add it if
		// there's a need identified.  One other curve shape we might want is "Square Root", which might optionally
		// support any positive fractional power (if we also supported any positive even number for an "Exponential"
		// type.
	};
}

UENUM()
namespace EEnvTestWeight
{
	enum Type
	{
		None,
		Square,
		Inverse,
		Unused			UMETA(Hidden),
		Constant,
		Skip			UMETA(DisplayName = "Do not weight"),
	};
}

UENUM()
namespace EEnvTestCost
{
	enum Type
	{
		/** Reading data, math operations (e.g. distance). */
		Low,
		/** Processing data from multiple sources (e.g. fire tickets). */
		Medium,
		/** Really expensive calls (e.g. visibility traces, pathfinding).  */
		High,
	};
}

UENUM()
namespace EEnvTestFilterOperator
{
	enum Type
	{
		AllPass			UMETA(Tooltip = "All contexts must pass condition"),
		AnyPass			UMETA(Tooltip = "At least one context must pass condition"),
	};
}

UENUM()
namespace EEnvTestScoreOperator
{
	enum Type
	{
		AverageScore	UMETA(Tooltip = "Use average score from all contexts"),
		MinScore		UMETA(Tooltip = "Use minimum score from all contexts"),
		MaxScore		UMETA(Tooltip = "Use maximum score from all contexts"),
	};
}

namespace EEnvItemStatus
{
	enum Type
	{
		Passed,
		Failed,
	};
}

UENUM()
namespace EEnvQueryStatus
{
	enum Type
	{
		Processing,
		Success,
		Failed,
		Aborted,
		OwnerLost,
		MissingParam,
	};
}

UENUM()
namespace EEnvQueryRunMode
{
	enum Type
	{
		SingleResult	UMETA(Tooltip="Pick first item with the best score", DisplayName="Single Best Item"),
		RandomBest5Pct	UMETA(Tooltip="Pick random item with score 95% .. 100% of max", DisplayName="Single Random Item from Best 5%"),
		RandomBest25Pct	UMETA(Tooltip="Pick random item with score 75% .. 100% of max", DisplayName="Single Random Item from Best 25%"),
		AllMatching		UMETA(Tooltip="Get all items that match conditions"),
	};
}

UENUM()
namespace EEnvQueryParam
{
	enum Type
	{
		Float,
		Int,
		Bool,
	};
}

UENUM()
enum class EAIParamType : uint8
{
	Float,
	Int,
	Bool,
};

UENUM()
namespace EEnvQueryTrace
{
	enum Type
	{
		None,
		Navigation,
		Geometry,
	};
}

UENUM()
namespace EEnvTraceShape
{
	enum Type
	{
		Line,
		Box,
		Sphere,
		Capsule,
	};
}

UENUM()
namespace EEnvOverlapShape
{
	enum Type
	{
		Box,
		Sphere,
		Capsule,
	};
}

UENUM()
namespace EEnvDirection
{
	enum Type
	{
		TwoPoints	UMETA(DisplayName="Two Points",ToolTip="Direction from location of one context to another."),
		Rotation	UMETA(ToolTip="Context's rotation will be used as a direction."),
	};
}

UENUM()
namespace EEnvQueryTestClamping
{
	enum Type
	{
		None,			
		/** Clamp to value specified in test. */
		SpecifiedValue,
		/** Clamp to test's filter threshold. */
		FilterThreshold
	};
}

USTRUCT(BlueprintType)
struct AIMODULE_API FEnvNamedValue
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Param)
	FName ParamName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Param)
	TEnumAsByte<EAIParamType> ParamType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Param)
	float Value;
};

USTRUCT()
struct AIMODULE_API FEnvDirection
{
	GENERATED_USTRUCT_BODY()

	/** line A: start context */
	UPROPERTY(EditDefaultsOnly, Category=Direction)
	TSubclassOf<UEnvQueryContext> LineFrom;

	/** line A: finish context */
	UPROPERTY(EditDefaultsOnly, Category=Direction)
	TSubclassOf<UEnvQueryContext> LineTo;

	/** line A: direction context */
	UPROPERTY(EditDefaultsOnly, Category=Direction)
	TSubclassOf<UEnvQueryContext> Rotation;

	/** defines direction of second line used by test */
	UPROPERTY(EditDefaultsOnly, Category=Direction, meta=(DisplayName="Mode"))
	TEnumAsByte<EEnvDirection::Type> DirMode;

	FText ToText() const;
};

USTRUCT()
struct AIMODULE_API FEnvTraceData
{
	GENERATED_USTRUCT_BODY()

	enum EDescriptionMode
	{
		Brief,
		Detailed,
	};

	FEnvTraceData() : 
		ProjectDown(1024.0f), ProjectUp(1024.0f), ExtentX(10.0f), ExtentY(10.0f), ExtentZ(10.0f), bOnlyBlockingHits(true),
		bCanTraceOnNavMesh(true), bCanTraceOnGeometry(true), bCanDisableTrace(true), bCanProjectDown(false)
	{
	}

	/** version number for updates */
	UPROPERTY()
	int32 VersionNum;

	/** navigation filter for tracing */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	TSubclassOf<UNavigationQueryFilter> NavigationFilter;

	/** search height: below point */
	UPROPERTY(EditDefaultsOnly, Category=Trace, meta=(UIMin=0, ClampMin=0))
	float ProjectDown;

	/** search height: above point */
	UPROPERTY(EditDefaultsOnly, Category=Trace, meta=(UIMin=0, ClampMin=0))
	float ProjectUp;

	/** shape parameter for trace */
	UPROPERTY(EditDefaultsOnly, Category=Trace, meta=(UIMin=0, ClampMin=0))
	float ExtentX;

	/** shape parameter for trace */
	UPROPERTY(EditDefaultsOnly, Category=Trace, meta=(UIMin=0, ClampMin=0))
	float ExtentY;

	/** shape parameter for trace */
	UPROPERTY(EditDefaultsOnly, Category=Trace, meta=(UIMin=0, ClampMin=0))
	float ExtentZ;

	/** this value will be added to resulting location's Z axis. Can be useful when 
	 *	projecting points to navigation since navmesh is just an approximation of level 
	 *	geometry and items may end up being under collide-able geometry which would 
	 *	for example falsify visibility tests.*/
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	float PostProjectionVerticalOffset;

	/** geometry trace channel */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	TEnumAsByte<enum ETraceTypeQuery> TraceChannel;

	/** geometry trace channel for serialization purposes */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	TEnumAsByte<enum ECollisionChannel> SerializedChannel;

	/** shape used for geometry tracing */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	TEnumAsByte<EEnvTraceShape::Type> TraceShape;

	/** shape used for geometry tracing */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	TEnumAsByte<EEnvQueryTrace::Type> TraceMode;

	/** if set, trace will run on complex collisions */
	UPROPERTY(EditDefaultsOnly, Category=Trace, AdvancedDisplay)
	uint32 bTraceComplex : 1;

	/** if set, trace will look only for blocking hits */
	UPROPERTY(EditDefaultsOnly, Category=Trace, AdvancedDisplay)
	uint32 bOnlyBlockingHits : 1;

	/** if set, editor will allow picking navmesh trace */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	uint32 bCanTraceOnNavMesh : 1;

	/** if set, editor will allow picking geometry trace */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	uint32 bCanTraceOnGeometry : 1;

	/** if set, editor will allow  */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	uint32 bCanDisableTrace : 1;

	/** if set, editor show height up/down properties for projection */
	UPROPERTY(EditDefaultsOnly, Category=Trace)
	uint32 bCanProjectDown : 1;

	FText ToText(EDescriptionMode DescMode) const;

	void SetGeometryOnly();
	void SetNavmeshOnly();
	
	void OnPostLoad();
};

USTRUCT()
struct AIMODULE_API FEnvOverlapData
{
	GENERATED_USTRUCT_BODY()

	FEnvOverlapData() :
		ExtentX(10.0f),
		ExtentY(10.0f),
		ExtentZ(10.0f),
		ShapeOffset(FVector::ZeroVector),
		bOnlyBlockingHits(true),
		bOverlapComplex(false)
	{
	}

	/** shape parameter for overlap */
	UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	float ExtentX;

	/** shape parameter for overlap */
	UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	float ExtentY;

	/** shape parameter for overlap */
	UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	float ExtentZ;

	UPROPERTY(EditDefaultsOnly, Category = Trace, AdvancedDisplay, Meta =
		(Tooltip="Offset from the item location at which to test the overlap.  For example, you may need to offset vertically to avoid overlaps with flat ground."))
	FVector ShapeOffset;

	/** geometry trace channel used for overlap */
	UPROPERTY(EditDefaultsOnly, Category = Overlap)
	TEnumAsByte<enum ECollisionChannel> OverlapChannel;

	/** shape used for geometry overlap */
	UPROPERTY(EditDefaultsOnly, Category = Overlap)
	TEnumAsByte<EEnvOverlapShape::Type> OverlapShape;

	/** if set, overlap will look only for blocking hits */
	UPROPERTY(EditDefaultsOnly, Category = Overlap, AdvancedDisplay)
	uint32 bOnlyBlockingHits : 1;

	/** if set, overlap will run on complex collisions */
	UPROPERTY(EditDefaultsOnly, Category = Overlap, AdvancedDisplay)
	uint32 bOverlapComplex : 1;
};

//////////////////////////////////////////////////////////////////////////
// Returned results

struct AIMODULE_API FEnvQueryItem
{
	/** total score of item */
	float Score;

	/** raw data offset */
	int32 DataOffset:31;

	/** has this item been discarded? */
	uint32 bIsDiscarded:1;

	FORCEINLINE bool IsValid() const { return DataOffset >= 0 && !bIsDiscarded; }
	FORCEINLINE void Discard() { bIsDiscarded = true; }

	bool operator<(const FEnvQueryItem& Other) const
	{
		// sort by validity
		if (IsValid() != Other.IsValid())
		{
			// self not valid = less important
			return !IsValid();
		}

		// sort by score if not equal. As last resort sort by DataOffset to achieve stable sort.
		return Score != Other.Score ? Score < Other.Score : DataOffset < Other.DataOffset;
	}

	FEnvQueryItem() : Score(0.0f), DataOffset(-1), bIsDiscarded(false) {}
	FEnvQueryItem(int32 InOffset) : Score(0.0f), DataOffset(InOffset), bIsDiscarded(false) {}
};

template <> struct TIsZeroConstructType<FEnvQueryItem> { enum { Value = true }; };

USTRUCT()
struct AIMODULE_API FEnvQueryResult
{
	GENERATED_USTRUCT_BODY()

	TArray<FEnvQueryItem> Items;

	/** type of generated items */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	TSubclassOf<UEnvQueryItemType> ItemType;

	/** raw data of items */
	TArray<uint8> RawData;

private:
	/** query status */
	EEnvQueryStatus::Type Status;

public:
	/** index of query option, that generated items */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	int32 OptionIndex;

	/** instance ID */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	int32 QueryID;

	/** instance owner. Mind that it doesn't have to be the query's "Querier". This is just the object that is responsible for this query instance. */
	TWeakObjectPtr<UObject> Owner;

	FORCEINLINE float GetItemScore(int32 Index) const { return Items.IsValidIndex(Index) ? Items[Index].Score : 0.0f; }

	/** item accessors for basic types */
	AActor* GetItemAsActor(int32 Index) const;
	FVector GetItemAsLocation(int32 Index) const;

	/** note that this function does not strip out the null-actors to not mess up results of GetItemScore(Index) calls*/
	void GetAllAsActors(TArray<AActor*>& OutActors) const;
	void GetAllAsLocations(TArray<FVector>& OutLocations) const;

	FEnvQueryResult() : ItemType(NULL), Status(EEnvQueryStatus::Processing), OptionIndex(0) {}
	FEnvQueryResult(const EEnvQueryStatus::Type& InStatus) : ItemType(NULL), Status(InStatus), OptionIndex(0) {}

	FORCEINLINE bool IsFinished() const { return Status != EEnvQueryStatus::Processing; }
	FORCEINLINE bool IsAborted() const { return Status == EEnvQueryStatus::Aborted; }
	FORCEINLINE void MarkAsMissingParam() { Status = EEnvQueryStatus::MissingParam; }
	FORCEINLINE void MarkAsAborted() { Status = EEnvQueryStatus::Aborted; }
	FORCEINLINE void MarkAsFailed() { Status = EEnvQueryStatus::Failed; }
	FORCEINLINE void MarkAsFinishedWithoutIssues() { Status = EEnvQueryStatus::Success; }
	FORCEINLINE void MarkAsOwnerLost() { Status = EEnvQueryStatus::OwnerLost; }

	FORCEINLINE EEnvQueryStatus::Type GetRawStatus() const { return Status; }
};


//////////////////////////////////////////////////////////////////////////
// Runtime processing structures

DECLARE_DELEGATE_OneParam(FQueryFinishedSignature, TSharedPtr<FEnvQueryResult>);

struct AIMODULE_API FEnvQuerySpatialData
{
	FVector Location;
	FRotator Rotation;
};

/** Detailed information about item, used by tests */
struct AIMODULE_API FEnvQueryItemDetails
{
	/** Results assigned by option's tests, before any modifications */
	TArray<float> TestResults;

#if USE_EQS_DEBUGGER
	/** Results assigned by option's tests, after applying modifiers, normalization and weight */
	TArray<float> TestWeightedScores;

	int32 FailedTestIndex;
	int32 ItemIndex;
	FString FailedDescription;
#endif // USE_EQS_DEBUGGER

	FEnvQueryItemDetails() {}
	FEnvQueryItemDetails(int32 NumTests, int32 InItemIndex)
	{
		TestResults.AddZeroed(NumTests);
#if USE_EQS_DEBUGGER
		TestWeightedScores.AddZeroed(NumTests);
		ItemIndex = InItemIndex;
		FailedTestIndex = INDEX_NONE;
#endif
	}

	FORCEINLINE uint32 GetAllocatedSize() const
	{
		return sizeof(*this) +
#if USE_EQS_DEBUGGER
			TestWeightedScores.GetAllocatedSize() +
#endif
			TestResults.GetAllocatedSize();
	}
};

struct AIMODULE_API FEnvQueryContextData
{
	/** type of context values */
	TSubclassOf<UEnvQueryItemType> ValueType;

	/** number of stored values */
	int32 NumValues;

	/** data of stored values */
	TArray<uint8> RawData;

	FEnvQueryContextData()
		: NumValues(0)
	{}

	FORCEINLINE uint32 GetAllocatedSize() const { return sizeof(*this) + RawData.GetAllocatedSize(); }
};

struct AIMODULE_API FEnvQueryOptionInstance
{
	/** generator object, raw pointer can be used safely because it will be always referenced by EnvQueryManager */
	UEnvQueryGenerator* Generator;

	/** test objects, raw pointer can be used safely because it will be always referenced by EnvQueryManager */
	TArray<UEnvQueryTest*> Tests;

	/** type of generated items */
	TSubclassOf<UEnvQueryItemType> ItemType;

	/** if set, items will be shuffled after tests */
	uint32 bHasNavLocations : 1;

	FORCEINLINE uint32 GetAllocatedSize() const { return sizeof(*this) + Tests.GetAllocatedSize(); }
};

#if NO_LOGGING
#define EQSHEADERLOG(...)
#else
#define EQSHEADERLOG(msg) Log(msg)
#endif // NO_LOGGING

struct FEQSQueryDebugData
{
	TArray<FEnvQueryItem> DebugItems;
	TArray<FEnvQueryItemDetails> DebugItemDetails;
	TArray<uint8> RawData;
	TArray<FString> PerformedTestNames;
	// indicates the query was run in a single-item mode and that it has been found
	uint32 bSingleItemResult : 1;

	void Store(const FEnvQueryInstance* QueryInstance);
	void Reset()
	{
		DebugItems.Reset();
		DebugItemDetails.Reset();
		RawData.Reset();
		PerformedTestNames.Reset();
		bSingleItemResult = false;
	}
};

// BEGIN DEPRECATED SUPPORT

USTRUCT()
struct AIMODULE_API FEnvFloatParam_DEPRECATED
{
	GENERATED_USTRUCT_BODY();

	/** default value */
	UPROPERTY(EditDefaultsOnly, Category = Param)
	float Value;

	/** name of parameter */
	UPROPERTY(EditDefaultsOnly, Category = Param)
	FName ParamName;

	bool IsNamedParam() const { return ParamName != NAME_None; }
	void Convert(UObject* Owner, FAIDataProviderFloatValue& ValueProvider);
};

USTRUCT()
struct AIMODULE_API FEnvIntParam_DEPRECATED
{
	GENERATED_USTRUCT_BODY();

	/** default value */
	UPROPERTY(EditDefaultsOnly, Category = Param)
	int32 Value;

	/** name of parameter */
	UPROPERTY(EditDefaultsOnly, Category = Param)
	FName ParamName;

	bool IsNamedParam() const { return ParamName != NAME_None; }
	void Convert(UObject* Owner, FAIDataProviderIntValue& ValueProvider);
};

USTRUCT()
struct AIMODULE_API FEnvBoolParam_DEPRECATED
{
	GENERATED_USTRUCT_BODY();

	/** default value */
	UPROPERTY(EditDefaultsOnly, Category = Param)
	bool Value;

	/** name of parameter */
	UPROPERTY(EditDefaultsOnly, Category = Param)
	FName ParamName;

	bool IsNamedParam() const { return ParamName != NAME_None; }
	void Convert(UObject* Owner, FAIDataProviderBoolValue& ValueProvider);
};

USTRUCT()
struct DEPRECATED(4.8, "FEnvFloatParam is deprecated in 4.8 and was replaced with FAIDataProviderFloatValue. Please use that type instead.") AIMODULE_API FEnvFloatParam : public FEnvFloatParam_DEPRECATED
{
	GENERATED_USTRUCT_BODY();
};

USTRUCT()
struct DEPRECATED(4.8, "FEnvIntParam is deprecated in 4.8 and was replaced with FAIDataProviderIntValue. Please use that type instead.") AIMODULE_API FEnvIntParam : public FEnvIntParam_DEPRECATED
{
	GENERATED_USTRUCT_BODY();
};

USTRUCT()
struct DEPRECATED(4.8, "FEnvBoolParam is deprecated in 4.8 and was replaced with FAIDataProviderBoolValue. Please use that type instead.") AIMODULE_API FEnvBoolParam : public FEnvBoolParam_DEPRECATED
{
	GENERATED_USTRUCT_BODY();
};

// END DEPRECATED SUPPORT

UCLASS(Abstract)
class AIMODULE_API UEnvQueryTypes : public UObject
{
	GENERATED_BODY()

public:
	/** special test value assigned to items skipped by condition check */
	static float SkippedItemValue;

	static FText GetShortTypeName(const UObject* Ob);
	static FText DescribeContext(TSubclassOf<UEnvQueryContext> ContextClass);
};

struct AIMODULE_API FEnvQueryInstance : public FEnvQueryResult
{
	typedef float FNamedParamValueType;

	/** short name of query template - friendly name for debugging */
	FString QueryName;

	/** unique name of query template - object name */
	FName UniqueName;

	/** world owning this query instance */
	UWorld* World;

	/** observer's delegate */
	FQueryFinishedSignature FinishDelegate;

	/** execution params */
	TMap<FName, FNamedParamValueType> NamedParams;

	/** contexts in use */
	TMap<UClass*, FEnvQueryContextData> ContextCache;

	/** list of options */
	TArray<FEnvQueryOptionInstance> Options;

	/** currently processed test (-1 = generator) */
	int32 CurrentTest;

	/** non-zero if test run last step has been stopped mid-process. This indicates
	 *	index of the first item that needs processing when resumed */
	int32 CurrentTestStartingItem;

	/** list of item details */
	TArray<FEnvQueryItemDetails> ItemDetails;

	/** number of valid items on list */
	int32 NumValidItems;

	/** size of current value */
	uint16 ValueSize;

	/** used to breaking from item iterator loops */
	uint8 bFoundSingleResult : 1;

private:
	/** set when testing final condition of an option */
	uint8 bPassOnSingleResult : 1;

	/** true if this query has logged a warning that it overran the time limit */
	uint8 bHasLoggedTimeLimitWarning : 1;

	/** platform time when this query was started */
	double StartTime;

	/** if > 0 then it's how much time query has for performing current step */
	double CurrentStepTimeLimit;

	/** time spent executing this query */
	double TotalExecutionTime;

	/** time spent doing generation for this query */
	double GenerationExecutionTime;

	// @todo do we really need this data in shipped builds?
	/** time spent on each test of this query */
	TArray<double> PerStepExecutionTime;

public:
#if USE_EQS_DEBUGGER
	/** set to true to store additional debug info */
	uint8 bStoreDebugInfo : 1;
#endif // USE_EQS_DEBUGGER

	/** run mode */
	EEnvQueryRunMode::Type Mode;

	/** item type's CDO for location tests */
	UEnvQueryItemType_VectorBase* ItemTypeVectorCDO;

	/** item type's CDO for actor tests */
	UEnvQueryItemType_ActorBase* ItemTypeActorCDO;

	FEnvQueryInstance();
	FEnvQueryInstance(const FEnvQueryInstance& Other);
	~FEnvQueryInstance();

	/** execute single step of query */
	void ExecuteOneStep(double InCurrentStepTimeLimit);

	/** set when we started the query */
	void SetQueryStartTime() { StartTime = FPlatformTime::Seconds(); }

	/** get when we started the query */
	double GetQueryStartTime() const { return StartTime; }

	/** have we logged that we overran time the time limit? */
	bool HasLoggedTimeLimitWarning() const { return !!bHasLoggedTimeLimitWarning; }

	/** set that we logged that we overran time the time limit. */
	void SetHasLoggedTimeLimitWarning() { bHasLoggedTimeLimitWarning = true; }

	/** get the amount of time spent executing query */
	double GetTotalExecutionTime() const { return TotalExecutionTime; }

	/** describe for logging purposes what the query spent time on */
	FString GetExecutionTimeDescription() const;

	/** update context cache */
	bool PrepareContext(UClass* Context, FEnvQueryContextData& ContextData);

	/** helpers for reading spatial data from context */
	bool PrepareContext(UClass* Context, TArray<FEnvQuerySpatialData>& Data);
	bool PrepareContext(UClass* Context, TArray<FVector>& Data);
	bool PrepareContext(UClass* Context, TArray<FRotator>& Data);
	/** helpers for reading actor data from context */
	bool PrepareContext(UClass* Context, TArray<AActor*>& Data);
	
	bool IsInSingleItemFinalSearch() const { return !!bPassOnSingleResult; }
	/** check if current test can batch its calculations */
	bool CanBatchTest() const { return !IsInSingleItemFinalSearch(); }

	/** raw data operations */
	void ReserveItemData(int32 NumAdditionalItems);

	template<typename TypeItem, typename TypeValue>
	void AddItemData(TypeValue ItemValue)
	{
		DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, RawData.GetAllocatedSize() + Items.GetAllocatedSize());

		check(GetDefault<TypeItem>()->GetValueSize() == sizeof(TypeValue));
		check(GetDefault<TypeItem>()->GetValueSize() == ValueSize);
		const int32 DataOffset = RawData.AddUninitialized(ValueSize);
		TypeItem::SetValue(RawData.GetData() + DataOffset, ItemValue);
		Items.Add(FEnvQueryItem(DataOffset));

		INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, RawData.GetAllocatedSize() + Items.GetAllocatedSize());
	}

	/** AddItemData specialization for arrays if values */
	template<typename TypeItem, typename TypeValue>
	void AddItemData(TArray<TypeValue>& ItemCollection)
	{
		if (ItemCollection.Num() > 0)
		{
			DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, RawData.GetAllocatedSize() + Items.GetAllocatedSize());

			check(GetDefault<TypeItem>()->GetValueSize() == sizeof(TypeValue));
			check(GetDefault<TypeItem>()->GetValueSize() == ValueSize);
			int32 DataOffset = RawData.AddUninitialized(ValueSize * ItemCollection.Num());
			Items.Reserve(Items.Num() + ItemCollection.Num());

			for (TypeValue& Item : ItemCollection)
			{
				TypeItem::SetValue(RawData.GetData() + DataOffset, Item);
				Items.Add(FEnvQueryItem(DataOffset));
				DataOffset += ValueSize;
			}

			INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, RawData.GetAllocatedSize() + Items.GetAllocatedSize());
		}
	}

protected:

	/** prepare item data after generator has finished */
	void FinalizeGeneration();

	/** update costs and flags after test has finished */
	void FinalizeTest();
	
	/** final pass on items of finished query */
	void FinalizeQuery();

	/** normalize total score in range 0..1 */
	void NormalizeScores();

	/** sort all scores, from highest to lowest */
	void SortScores();

	/** pick one of items with score equal or higher than specified */
	void PickRandomItemOfScoreAtLeast(float MinScore);

	/** discard all items but one */
	void PickSingleItem(int32 ItemIndex);

public:

	/** removes all runtime data that can be used for debugging (not a part of actual query result) */
	void StripRedundantData();

#if STATS
	FORCEINLINE void IncStats()
	{
		INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, GetAllocatedSize());
		INC_DWORD_STAT_BY(STAT_AI_EQS_NumItems, Items.Num());
	}

	FORCEINLINE void DecStats()
	{
		DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, GetAllocatedSize()); 
		DEC_DWORD_STAT_BY(STAT_AI_EQS_NumItems, Items.Num());
	}

	uint32 GetAllocatedSize() const;
	uint32 GetContextAllocatedSize() const;
#else
	FORCEINLINE uint32 GetAllocatedSize() const { return 0; }
	FORCEINLINE uint32 GetContextAllocatedSize() const { return 0; }
	FORCEINLINE void IncStats() {}
	FORCEINLINE void DecStats() {}
#endif // STATS

#if !NO_LOGGING
	void Log(const FString Msg) const;
#endif // #if !NO_LOGGING

#if USE_EQS_DEBUGGER
#	define  UE_EQS_DBGMSG(Condition, Format, ...) \
					if (Condition) \
					{ \
						Instance->ItemDetails[CurrentItem].FailedDescription = FString::Printf(Format, ##__VA_ARGS__); \
					}

#	define UE_EQS_LOG(CategoryName, Verbosity, Format, ...) \
					UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); \
					UE_EQS_DBGMSG(true, Format, ##__VA_ARGS__); 
#else
#	define UE_EQS_DBGMSG(Condition, Format, ...)
#	define UE_EQS_LOG(CategoryName, Verbosity, Format, ...) UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); 
#endif

#if CPP || UE_BUILD_DOCS
	struct AIMODULE_API ItemIterator
	{
		ItemIterator(const UEnvQueryTest* QueryTest, FEnvQueryInstance& QueryInstance, int32 StartingItemIndex = INDEX_NONE);

		~ItemIterator()
		{
			Instance->CurrentTestStartingItem = CurrentItem;
		}

		/** Filter and score an item - used by tests working on float values
		 *  (can be called multiple times for single item when processing contexts with multiple entries)
		 *  NOTE: The Score is the raw score, before clamping, normalizing, and multiplying by weight.  The FilterMin
		 *  and FilterMax values are ONLY used for filtering (if any).
		 */
		void SetScore(EEnvTestPurpose::Type TestPurpose, EEnvTestFilterType::Type FilterType, float Score, float FilterMin, float FilterMax)
		{
			if (bForced)
			{
				return;
			}

			bool bPassedTest = true;

			if (TestPurpose != EEnvTestPurpose::Score)	// May need to filter results!
			{
				switch (FilterType)
				{
					case EEnvTestFilterType::Maximum:
						bPassedTest = (Score <= FilterMax);
						UE_EQS_DBGMSG(!bPassedTest, TEXT("Value %f is above maximum value set to %f"), Score, FilterMax);
						break;

					case EEnvTestFilterType::Minimum:
						bPassedTest = (Score >= FilterMin);
						UE_EQS_DBGMSG(!bPassedTest, TEXT("Value %f is below minimum value set to %f"), Score, FilterMin);
						break;

					case EEnvTestFilterType::Range:
						bPassedTest = (Score >= FilterMin) && (Score <= FilterMax);
						UE_EQS_DBGMSG(!bPassedTest, TEXT("Value %f is out of range set to (%f, %f)"), Score, FilterMin, FilterMax);
						break;

					case EEnvTestFilterType::Match:
						UE_EQS_LOG(LogEQS, Error, TEXT("Filtering Type set to 'Match' for floating point test.  Will consider test as failed in all cases."));
						bPassedTest = false;
						break;

					default:
						UE_EQS_LOG(LogEQS, Error, TEXT("Filtering Type set to invalid value for floating point test.  Will consider test as failed in all cases."));
						bPassedTest = false;
						break;
				}
			}

			if (bPassedTest)
			{
				SetScoreInternal(Score);
				NumPassedForItem++;
			}

			NumTestsForItem++;
		}

		/** Filter and score an item - used by tests working on bool values
		 *  (can be called multiple times for single item when processing contexts with multiple entries)
		 */
		void SetScore(EEnvTestPurpose::Type TestPurpose, EEnvTestFilterType::Type FilterType, bool bScore, bool bExpected)
		{
			if (bForced)
			{
				return;
			}

			bool bPassedTest = true;
			switch (FilterType)
			{
				case EEnvTestFilterType::Match:
					bPassedTest = (bScore == bExpected);
					UE_EQS_DBGMSG(!bPassedTest, TEXT("Boolean score don't mach (expected %s and got %s)"), bExpected ? TEXT("TRUE") : TEXT("FALSE"), bScore ? TEXT("TRUE") : TEXT("FALSE"));
					break;

				case EEnvTestFilterType::Maximum:
					UE_EQS_LOG(LogEQS, Error, TEXT("Filtering Type set to 'Maximum' for boolean test.  Will consider test as failed in all cases."));
					bPassedTest = false;
					break;

				case EEnvTestFilterType::Minimum:
					UE_EQS_LOG(LogEQS, Error, TEXT("Filtering Type set to 'Minimum' for boolean test.  Will consider test as failed in all cases."));
					bPassedTest = false;
					break;

				case EEnvTestFilterType::Range:
					UE_EQS_LOG(LogEQS, Error, TEXT("Filtering Type set to 'Range' for boolean test.  Will consider test as failed in all cases."));
					bPassedTest = false;
					break;

				default:
					UE_EQS_LOG(LogEQS, Error, TEXT("Filtering Type set to invalid value for boolean test.  Will consider test as failed in all cases."));
					bPassedTest = false;
					break;
			}

			if (bPassedTest)
			{
				SetScoreInternal(1.0f);
				NumPassedForItem++;
			}

			NumTestsForItem++;
		}

		uint8* GetItemData()
		{
			return Instance->RawData.GetData() + Instance->Items[CurrentItem].DataOffset;
		}

		/** Force state and score of item
		 *  Any following SetScore calls for current item will be ignored
		 */
		void ForceItemState(EEnvItemStatus::Type InStatus, float Score = UEnvQueryTypes::SkippedItemValue)
		{
			bForced = true;
			bPassed = (InStatus == EEnvItemStatus::Passed);
			ItemScore = Score;
		}

		DEPRECATED(4.9, "This function is now deprecated, please use ForceItemState instead")
		void DiscardItem()
		{
			ForceItemState(EEnvItemStatus::Failed);
		}

		DEPRECATED(4.9, "This function is now deprecated, please use ForceItemState instead")
		void SkipItem()
		{
			ForceItemState(EEnvItemStatus::Passed);
		}

		/** Disables time slicing for this iterator, use with caution! */
		void IgnoreTimeLimit()
		{
			Deadline = -1.0f;
		}

		int32 GetIndex() const
		{
			return CurrentItem;
		}

		DEPRECATED(4.8, "This function is now deprecatewd, please use GetIndex() for current index or GetItemData() for raw data pointer")
		int32 operator*() const
		{
			return GetIndex();
		}

		FORCEINLINE explicit operator bool() const
		{
			return CurrentItem < Instance->Items.Num() && !Instance->bFoundSingleResult && (Deadline < 0 || FPlatformTime::Seconds() < Deadline);
		}

		void operator++()
		{
			StoreTestResult();
			if (!Instance->bFoundSingleResult)
			{
				InitItemScore();
				FindNextValidIndex();
			}
		}

	protected:

		FEnvQueryInstance* Instance;
		double Deadline;
		float ItemScore;
		int32 CurrentItem;
		int16 NumPassedForItem;
		int16 NumTestsForItem;
		uint8 CachedFilterOp;
		uint8 CachedScoreOp;
		uint8 bPassed : 1;
		uint8 bForced : 1;
		uint8 bIsFiltering : 1;

		void InitItemScore()
		{
			NumPassedForItem = 0;
			NumTestsForItem = 0;
			ItemScore = 0.0f;
			bPassed = true;
			bForced = false;
		}

		void HandleFailedTestResult();
		void StoreTestResult();

		FORCEINLINE void FindNextValidIndex()
		{
			for (CurrentItem++; CurrentItem < Instance->Items.Num() && !Instance->Items[CurrentItem].IsValid(); CurrentItem++)
				;
		}

		FORCEINLINE void SetScoreInternal(float Score)
		{
			switch (CachedScoreOp)
			{
			case EEnvTestScoreOperator::AverageScore:
				ItemScore += Score;
				break;

			case EEnvTestScoreOperator::MinScore:
				if (!NumPassedForItem || ItemScore > Score)
				{
					ItemScore = Score;
				}
				break;

			case EEnvTestScoreOperator::MaxScore:
				if (!NumPassedForItem || ItemScore < Score)
				{
					ItemScore = Score;
				}
				break;
			}
		}

		FORCEINLINE void CheckItemPassed()
		{
			if (!bForced)
			{
				if (!bIsFiltering)
				{
					bPassed = true;
				}
				else if (CachedFilterOp == EEnvTestFilterOperator::AllPass)
				{
					bPassed = bPassed && (NumPassedForItem == NumTestsForItem);
				}
				else
				{
					bPassed = bPassed && (NumPassedForItem > 0);
				}
			}
		}
	};
#endif

#undef UE_EQS_LOG
#undef UE_EQS_DBGMSG

#if USE_EQS_DEBUGGER
	FEQSQueryDebugData DebugData;
	static bool bDebuggingInfoEnabled;
#endif // USE_EQS_DEBUGGER

	FBox GetBoundingBox() const;
};

namespace FEQSHelpers
{
	AIMODULE_API const ANavigationData* FindNavigationDataForQuery(FEnvQueryInstance& QueryInstance);

#if WITH_RECAST
	DEPRECATED(4.8, "FindNavMeshForQuery is deprecated. Please use FindNavigationDataForQuery")
	AIMODULE_API const ARecastNavMesh* FindNavMeshForQuery(FEnvQueryInstance& QueryInstance);
#endif // WITH_RECAST
}

USTRUCT(BlueprintType)
struct AIMODULE_API FAIDynamicParam
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = EQS)
	FName ParamName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = EQS)
	TEnumAsByte<EAIParamType> ParamType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EQS)
	float Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EQS)
	FBlackboardKeySelector BBKey;

	FAIDynamicParam()
	{
		BBKey.AllowNoneAsValue(true);
	}

	void ConfigureBBKey(UObject &QueryOwner);

	static void GenerateConfigurableParamsFromNamedValues(UObject &QueryOwner, TArray<FAIDynamicParam>& OutQueryConfig, TArray<FEnvNamedValue>& InQueryParams);
};

USTRUCT()
struct FEQSParametrizedQueryExecutionRequest
{
	GENERATED_USTRUCT_BODY()

	FEQSParametrizedQueryExecutionRequest();

	UPROPERTY(Category = Node, EditAnywhere, meta = (EditCondition = "!bUseBBKeyForQueryTemplate"))
	UEnvQuery* QueryTemplate;

	UPROPERTY(Category = Node, EditAnywhere)
	TArray<FAIDynamicParam> QueryConfig;
	
	/** blackboard key storing an EQS query template */
	UPROPERTY(EditAnywhere, Category = Blackboard, meta = (EditCondition = "bUseBBKeyForQueryTemplate"))
	FBlackboardKeySelector EQSQueryBlackboardKey;

	/** determines which item will be stored (All = only first matching) */
	UPROPERTY(Category = Node, EditAnywhere)
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode;

	UPROPERTY(Category = Node, EditAnywhere, meta=(InlineEditConditionToggle))
	uint32 bUseBBKeyForQueryTemplate : 1;

	uint32 bInitialized : 1;

	void InitForOwnerAndBlackboard(UObject& Owner, UBlackboardData* BBAsset);

	bool IsValid() const { return bInitialized; }

	int32 Execute(AActor& QueryOwner, const UBlackboardComponent* BlackboardComponent, FQueryFinishedSignature& QueryFinishedDelegate);

#if WITH_EDITOR
	void PostEditChangeProperty(UObject& Owner, struct FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR
};