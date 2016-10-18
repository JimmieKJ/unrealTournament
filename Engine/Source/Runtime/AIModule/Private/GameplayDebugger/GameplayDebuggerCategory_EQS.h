// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"

class FGameplayDebuggerCategory_EQS : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_EQS();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void OnDataPackReplicated(int32 DataPackId) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;
	virtual FDebugRenderSceneProxy* CreateSceneProxy(const UPrimitiveComponent* InComponent) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
#if USE_EQS_DEBUGGER
	struct FRepData
	{
		TArray<EQSDebug::FQueryData> QueryDebugData;
		
		void Serialize(FArchive& Ar);
	};
	FRepData DataPack;

	void DrawLookedAtItem(const EQSDebug::FQueryData& QueryData, APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) const;
	void DrawDetailedItemTable(const EQSDebug::FQueryData& QueryData, FGameplayDebuggerCanvasContext& CanvasContext) const;
	void DrawDetailedItemRow(const EQSDebug::FItemData& ItemData, FGameplayDebuggerCanvasContext& CanvasContext) const;
#endif

	void CycleShownQueries();

	uint32 bDrawLabels : 1;
	uint32 bDrawFailedItems : 1;

	int32 MaxItemTableRows;
	int32 MaxQueries;
	int32 ShownQueryIndex;
};

#endif // WITH_GAMEPLAY_DEBUGGER
