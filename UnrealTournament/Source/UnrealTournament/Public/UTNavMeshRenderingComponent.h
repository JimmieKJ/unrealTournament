// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AI/Navigation/NavMeshRenderingComponent.h"

#include "UTNavMeshRenderingComponent.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTNavMeshRenderingComponent : public UNavMeshRenderingComponent
{
	GENERATED_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GatherTriangleData(struct FNavMeshSceneProxyData* CurrentData) const;
};