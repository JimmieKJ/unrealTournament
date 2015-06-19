// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawService.h"
#include "EnvQueryTypes.h"
#include "EnvQueryDebugHelpers.h"
#include "EQSRenderingComponent.generated.h"

class IEQSQueryResultSourceInterface;
struct FEnvQueryInstance;

class AIMODULE_API FEQSSceneProxy : public FDebugRenderSceneProxy
{
public:
	FEQSSceneProxy(const UPrimitiveComponent* InComponent, const FString& ViewFlagName = TEXT("DebugAI"));
	FEQSSceneProxy(const UPrimitiveComponent* InComponent, const FString& ViewFlagName, const TArray<FSphere>& Spheres, const TArray<FText3d>& Texts);

	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override;
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override;

#if  USE_EQS_DEBUGGER 
	static void CollectEQSData(const UPrimitiveComponent* InComponent, const IEQSQueryResultSourceInterface* QueryDataSource, TArray<FSphere>& Spheres, TArray<FText3d>& Texts, TArray<EQSDebug::FDebugHelper>& DebugItems);
	static void CollectEQSData(const FEnvQueryResult* ResultItems, const FEnvQueryInstance* QueryInstance, float HighlightRangePct, bool ShouldDrawFailedItems, TArray<FSphere>& Spheres, TArray<FText3d>& Texts, TArray<EQSDebug::FDebugHelper>& DebugItems);
#endif
private:
	FEnvQueryResult QueryResult;	
	// can be 0
	AActor* ActorOwner;
	const IEQSQueryResultSourceInterface* QueryDataSource;
	uint32 bDrawOnlyWhenSelected : 1;

	static const FVector ItemDrawRadius;

	bool SafeIsActorSelected() const;
};

UCLASS(hidecategories=Object)
class AIMODULE_API UEQSRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	FString DrawFlagName;
	uint32 bDrawOnlyWhenSelected : 1;

#if  USE_EQS_DEBUGGER || ENABLE_VISUAL_LOG
	EQSDebug::FQueryData DebugData;
#endif

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
};
