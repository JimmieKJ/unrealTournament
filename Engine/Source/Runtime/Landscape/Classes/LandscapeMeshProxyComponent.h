// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTypes.h"
#include "StaticLighting.h"
#include "Components/PrimitiveComponent.h"

#include "LandscapeMeshProxyComponent.generated.h"

UCLASS(MinimalAPI)
class ULandscapeMeshProxyComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

private:
	/* The landscape this proxy was generated for */
	UPROPERTY()
	FGuid LandscapeGuid;

	/* The components this proxy was generated for */
	UPROPERTY()
	TArray<FIntPoint> ProxyComponentBases;

	/* LOD level proxy was generated for */
	UPROPERTY()
	int8 ProxyLOD;
public:
	void InitializeForLandscape(ALandscapeProxy* Landscape, int8 InProxyLOD)
	{
		LandscapeGuid = Landscape->GetLandscapeGuid();

		for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
		{
			if (Component)
			{
				ProxyComponentBases.Add(Component->GetSectionBase() / Component->ComponentSizeQuads);
			}
		}

		if (InProxyLOD != INDEX_NONE)
		{
			ProxyLOD = FMath::Clamp<int32>(InProxyLOD, 0, FMath::CeilLogTwo(Landscape->SubsectionSizeQuads + 1) - 1);
		}
	}
	
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};

