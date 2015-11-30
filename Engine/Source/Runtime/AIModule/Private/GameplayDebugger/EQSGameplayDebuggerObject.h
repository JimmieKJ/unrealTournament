// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Debug/GameplayDebuggerBaseObject.h"
#include "EQSGameplayDebuggerObject.generated.h"

UCLASS()
class UEQSGameplayDebuggerObject : public UGameplayDebuggerBaseObject, public IEQSQueryResultSourceInterface
{
	GENERATED_UCLASS_BODY()

	virtual FString GetCategoryName() const override { return TEXT("EQS"); };
	virtual void CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor) override;
	virtual void DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor) override;
	virtual class FDebugRenderSceneProxy* CreateSceneProxy(const UPrimitiveComponent* InComponent, UWorld* World, AActor* SelectedActor) override;

protected:
	/** local EQS debug data, decoded from EQSRepData blob */
#if  ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	TArray<EQSDebug::FQueryData> EQSLocalData;
#endif

	UFUNCTION()
	virtual void OnCycleDetailsView();

	void DrawEQSItemDetails(int32 ItemIdx, AActor* SelectedActor);

	// IEQSQueryResultSourceInterface start
	virtual const struct FEnvQueryResult* GetQueryResult() const override;
	virtual const struct FEnvQueryInstance* GetQueryInstance() const  override;

	virtual bool GetShouldDebugDrawLabels() const override { return bDrawEQSLabels; }
	virtual bool GetShouldDrawFailedItems() const override{ return bDrawEQSFailedItems; }
	// IEQSQueryResultSourceInterface end

	void OnDataReplicationComplited(const TArray<uint8>& ReplicatedData) override;

	uint32 bDrawEQSLabels : 1;
	uint32 bDrawEQSFailedItems : 1;
	int32 MaxEQSQueries;
	int32 CurrentEQSIndex;
	TSharedPtr<FEnvQueryInstance> CachedQueryInstance;
private:
	float ItemDescriptionWidth;
	float ItemScoreWidth;
	float TestScoreWidth;
	float LastDataCaptureTime;
};
