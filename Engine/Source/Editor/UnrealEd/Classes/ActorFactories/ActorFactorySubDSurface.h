// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ActorFactorySubDSurface.generated.h"

UCLASS(MinimalAPI, config=Editor)
class UActorFactorySubDSurface : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor ) override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
};



