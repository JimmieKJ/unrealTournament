// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "AI/Navigation/NavMeshRenderingComponent.h"

class FGameplayDebuggerCategory_Navmesh : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Navmesh();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual FDebugRenderSceneProxy* CreateSceneProxy(const UPrimitiveComponent* InComponent) override;
	virtual void OnDataPackReplicated(int32 DataPackId) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	FNavMeshSceneProxyData NavmeshRenderData;
};

#endif // WITH_GAMEPLAY_DEBUGGER
