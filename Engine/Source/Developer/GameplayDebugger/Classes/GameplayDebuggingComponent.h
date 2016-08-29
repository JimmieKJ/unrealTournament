// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////
// THIS CLASS IS NOW DEPRECATED AND WILL BE REMOVED IN NEXT VERSION
// Please check GameplayDebugger.h for details.

#pragma once
#include "Components/PrimitiveComponent.h"
#include "GameplayDebuggingTypes.h"
#include "EnvironmentQuery/EQSQueryResultSourceInterface.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"
#include "Debug/DebugDrawService.h"
#include "GameplayDebuggingComponent.generated.h"

#define WITH_EQS 1

struct FDebugContext;
//struct FEnvQueryInstance;

UENUM()
namespace EDebugComponentMessage
{
	enum Type
	{
		EnableExtendedView,
		DisableExtendedView, 
		ActivateReplication,
		DeactivateReplilcation,
		ActivateDataView,
		DeactivateDataView,
		SetMultipleDataViews,
	};
}

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDebuggingTargetChanged, class AActor* /*Owner of debugging component*/, bool /*is being debugged now*/);

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayDebugger, Log, All);

UENUM()
enum class EGameplayDebuggerShapeElement : uint8
{
	Invalid,
	String,
	SinglePoint,
	Segment,
	Box,
	Cone,
	Cylinder,
	Capsule,
	Polygon,
};

USTRUCT()
struct FGameplayDebuggerShapeElement
{
	GENERATED_USTRUCT_BODY()

	/** points defining shape */
	UPROPERTY()
	TArray<FVector> Points;

	UPROPERTY()
	float ThicknesOrRadius;

	/** description of shape */
	UPROPERTY()
	FString Description;

	/** color of shape */
	UPROPERTY()
	FColor Color;

	/** type of shape */
	UPROPERTY()
	EGameplayDebuggerShapeElement Type;

	FGameplayDebuggerShapeElement() : Type(EGameplayDebuggerShapeElement::Invalid) {}

	EGameplayDebuggerShapeElement GetType() const { return Type; }
	FColor GetFColor() const { return Color; }

	static FGameplayDebuggerShapeElement MakePoint(const FVector& Location, const float Radius, const FColor& Color, const FString& Description);
	static FGameplayDebuggerShapeElement MakeSegment(const FVector& StartLocation, const FVector& EndLocation, const float Thickness, const FColor& Color, const FString& Description);
	static FGameplayDebuggerShapeElement MakeBox(const FVector& Center, const FVector& Extent, const FColor& Color, const FString& Description);
	static FGameplayDebuggerShapeElement MakeCone(const FVector& Location, const FVector& Direction, const float Length, const FColor& Color, const FString& Description);
	static FGameplayDebuggerShapeElement MakeCylinder(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description);
	static FGameplayDebuggerShapeElement MakeCapsule(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description);
	static FGameplayDebuggerShapeElement MakePolygon(const TArray<FVector>& Verts, const FColor& Color, const FString& Description);
};

UCLASS(config=Engine)
class GAMEPLAYDEBUGGER_API UGameplayDebuggingComponent : public UPrimitiveComponent, public IEQSQueryResultSourceInterface
{
	GENERATED_UCLASS_BODY()

	friend class AGameplayDebuggingHUDComponent;

	struct FPathCorridorPolygons
	{
		TArray<FVector> Points;
		FColor Color;
	};

	UPROPERTY(globalconfig)
	FString DebugComponentClassName;

	UPROPERTY(Replicated)
	int32 ShowExtendedInformatiomCounter;

	UPROPERTY(Replicated)
	TArray<int32> ReplicateViewDataCounters;

	UPROPERTY(Replicated)
	FString ControllerName;

	UPROPERTY(Replicated)
	FString PawnName;

	UPROPERTY(Replicated)
	FString PawnClass;

	UPROPERTY(Replicated)
	FString DebugIcon;

	UPROPERTY(Replicated)
	FString MovementBaseInfo;

	UPROPERTY(Replicated)
	FString MovementModeInfo;

	UPROPERTY(Replicated)
	FString PathFollowingInfo;

	UPROPERTY(Replicated)
	FString CurrentAITask;

	UPROPERTY(Replicated)
	FString CurrentAIState;

	UPROPERTY(Replicated)
	FString CurrentAIAssets;

	UPROPERTY(Replicated)
	FString GameplayTasksState;

	UPROPERTY(Replicated)
	FString NavDataInfo;

	UPROPERTY(Replicated)
	FString AbilityInfo;

	UPROPERTY(Replicated)
	FString MontageInfo;

	UPROPERTY(Replicated)
	FString BrainComponentName;

	UPROPERTY(Replicated)
	FString BrainComponentString;

	UPROPERTY(ReplicatedUsing = OnRep_UpdateBlackboard)
	TArray<uint8> BlackboardRepData;

	FString BlackboardString;

	/** Begin path replication data */
	UPROPERTY(Replicated)
	TArray<FVector> PathPoints;

	UPROPERTY(ReplicatedUsing = OnRep_PathCorridorData)
	TArray<uint8> PathCorridorData;
	
	TArray<FPathCorridorPolygons> PathCorridorPolygons;
	/** End path replication data*/
	
	UPROPERTY(ReplicatedUsing = OnRep_UpdateNavmesh)
	TArray<uint8> NavmeshRepData;
	
	/** Begin EQS replication data */

	UPROPERTY(ReplicatedUsing = OnRep_UpdateEQS)
	TArray<uint8> EQSRepData;
	
	/** local EQS debug data, decoded from EQSRepData blob */
#if  USE_EQS_DEBUGGER || ENABLE_VISUAL_LOG
	TArray<EQSDebug::FQueryData> EQSLocalData;
#endif

	/** End EQS replication data */

	UPROPERTY(Replicated)
	int32 NextPathPointIndex;

	UPROPERTY(Replicated)
	uint32 bIsUsingPathFollowing : 1;

	UPROPERTY(Replicated)
	uint32 bIsUsingCharacter : 1;

	UPROPERTY(Replicated)
	uint32 bIsUsingBehaviorTree : 1;

	UPROPERTY(Replicated)
	uint32 bIsUsingAbilities : 1;

	uint32 bDrawEQSLabels:1;
	uint32 bDrawEQSFailedItems : 1;

	/** Start - Perception System */

	UPROPERTY(Replicated)
	FString PerceptionLegend;

	UPROPERTY(Replicated)
	float DistanceFromPlayer;
	
	UPROPERTY(Replicated)
	float DistanceFromSensor;

	UPROPERTY(Replicated)
	FVector SensingComponentLocation;

	UPROPERTY(Replicated)
	TArray<FGameplayDebuggerShapeElement> PerceptionShapeElements;
	/** End - Perception System */

	UFUNCTION()
	virtual void OnCycleDetailsView();

	UFUNCTION()
	virtual void OnRep_UpdateEQS();

	UFUNCTION()
	virtual void OnRep_UpdateBlackboard();

	virtual bool GetComponentClassCanReplicate() const override{ return true; }

	UFUNCTION()
	virtual void OnRep_UpdateNavmesh();

	UFUNCTION()
	virtual void OnRep_ActivationCounter();

	UFUNCTION(exec)
	void ServerReplicateData(uint32 InMessage, uint32 DataView);

	UFUNCTION(reliable, server, WithValidation)
	void ServerCollectNavmeshData(FVector_NetQuantize10 TargetLocation);

	UFUNCTION(reliable, server, WithValidation)
	void ServerDiscardNavmeshData();

	void PrepareNavMeshData(struct FNavMeshSceneProxyData*) const;

	UFUNCTION()
	virtual void OnRep_PathCorridorData();

	virtual void Activate(bool bReset=false) override;
	virtual void Deactivate() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void EnableDebugDraw(bool bEnable, bool InFocusedComponent = false);

	bool ShouldReplicateData(EAIDebugDrawDataView::Type InView) const { return ReplicateViewDataCounters[InView] > 0 /*true*/; }
	
#if !ENABLE_OLD_GAMEPLAY_DEBUGGER
	DEPRECATED_FORGAME(4.13, "GameplayDebuggingComponent class is now deprecated, please check GameplayDebugger.h for details.")
#endif // !ENABLE_OLD_GAMEPLAY_DEBUGGER
	virtual void CollectDataToReplicate(bool bCollectExtendedData);

	//=============================================================================
	// controller related stuff
	//=============================================================================
	UPROPERTY(Replicated)
	AActor* TargetActor;
	
	UFUNCTION(Reliable, Client, WithValidation)
	void ClientEnableTargetSelection(bool bEnable);

	void SetActorToDebug(AActor* Actor);
	FORCEINLINE AActor* GetSelectedActor() const
	{
		return TargetActor && TargetActor->IsPendingKill() == false ? TargetActor : nullptr;
	}

	void SetEQSIndex(int32 Index) { CurrentEQSIndex = Index; }
	int32 GetEQSIndex() { return CurrentEQSIndex; }
	//=============================================================================
	// EQS debugging
	//=============================================================================
	uint32 bEnableClientEQSSceneProxy : 1;

	void EnableClientEQSSceneProxy(bool bEnable) { bEnableClientEQSSceneProxy = bEnable;  MarkRenderStateDirty(); }
	bool IsClientEQSSceneProxyEnabled() { return bEnableClientEQSSceneProxy; }

	// IEQSQueryResultSourceInterface start
	virtual const struct FEnvQueryResult* GetQueryResult() const override;
	virtual const struct FEnvQueryInstance* GetQueryInstance() const  override;

	virtual bool GetShouldDebugDrawLabels() const override { return bDrawEQSLabels; }
	virtual bool GetShouldDrawFailedItems() const override{ return bDrawEQSFailedItems; }
	// IEQSQueryResultSourceInterface end
protected:
	virtual void CollectEQSData();
public:

	//=============================================================================
	// Rendering
	//=============================================================================
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	void SelectTargetToDebug();

protected:
#if WITH_RECAST
	ARecastNavMesh* GetNavData();
#endif

	virtual void CollectPathData();
	virtual void CollectBasicData();
	virtual void CollectBehaviorTreeData();
	virtual void CollectPerceptionData();

	virtual void CollectBasicMovementData(APawn* MyPawn);
	virtual void CollectBasicPathData(APawn* MyPawn);
	virtual void CollectBasicBehaviorData(APawn* MyPawn);
	virtual void CollectBasicAbilityData(APawn* MyPawn);
	virtual void CollectBasicAnimationData(APawn* MyPawn);

	FNavPathWeakPtr CurrentPath;
	float LastStoredPathTimeStamp;

	uint32 bEnabledTargetSelection : 1;
#if WITH_EDITOR
	uint32 bWasSelectedInEditor : 1;
#endif

	UPROPERTY(ReplicatedUsing = OnRep_ActivationCounter)
	uint8 ActivationCounter;

	float NextTargrtSelectionTime;
	/** navmesh data passed to rendering component */
	FBox NavMeshBounds;

	TWeakObjectPtr<APlayerController> PlayerOwner;

	int32 CurrentEQSIndex;
	TSharedPtr<FEnvQueryInstance> CachedQueryInstance;

public:
	static FName DefaultComponentName;
	static FOnDebuggingTargetChanged OnDebuggingTargetChangedDelegate;
};
