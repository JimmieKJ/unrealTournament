// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactorySubDSurface.generated.h"

class AActor;
class FAssetData;

UCLASS(MinimalAPI, config=Editor)
class UActorFactorySubDSurface : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor ) override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
};



