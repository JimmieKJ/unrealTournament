// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ActorFactoryBasicShape.generated.h"

UCLASS(MinimalAPI,config=Editor)
class UActorFactoryBasicShape : public UActorFactoryStaticMesh
{
	GENERATED_UCLASS_BODY()

	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;

	UNREALED_API static const FName BasicCube;
	UNREALED_API static const FName BasicSphere;
	UNREALED_API static const FName BasicCylinder;
	UNREALED_API static const FName BasicCone;
};
