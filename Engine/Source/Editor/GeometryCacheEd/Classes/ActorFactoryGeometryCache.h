// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEd.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryGeometryCache.generated.h"

/** Factory class for spawning and creating GeometryCacheActors */
UCLASS(MinimalAPI, config = Editor)
class UActorFactoryGeometryCache : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UActorFactory Interface
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual void PostCreateBlueprint(UObject* Asset, AActor* CDO) override;	
	// End UActorFactory Interface
};
