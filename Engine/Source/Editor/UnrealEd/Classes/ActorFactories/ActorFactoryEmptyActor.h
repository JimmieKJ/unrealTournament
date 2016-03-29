// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ActorFactoryEmptyActor.generated.h"

UCLASS(MinimalAPI,config=Editor)
class UActorFactoryEmptyActor : public UActorFactory
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;

protected:
	virtual AActor* SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags ObjectFlags, const FName Name ) override;

};
