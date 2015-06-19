// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryBoxVolume.generated.h"

UCLASS(MinimalAPI, config=Editor, collapsecategories, hidecategories=Object)
class UActorFactoryBoxVolume : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UActorFactory Interface
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	UNREALED_API virtual void PostSpawnActor( UObject* Asset, AActor* NewActor ) override;
	// End UActorFactory Interface
};
