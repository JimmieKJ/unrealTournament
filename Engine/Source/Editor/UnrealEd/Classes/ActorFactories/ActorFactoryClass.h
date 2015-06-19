// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryClass.generated.h"

UCLASS(MinimalAPI, config=Editor, collapsecategories, hidecategories=Object)
class UActorFactoryClass : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UActorFactory Interface
	virtual bool PreSpawnActor( UObject* Asset, FTransform& InOutLocation ) override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual AActor* GetDefaultActor( const FAssetData& AssetData ) override;
	// End UActorFactory Interface	

protected:
	virtual AActor* SpawnActor( UObject* Asset, ULevel* InLevel, const FVector& Location, const FRotator& Rotation, EObjectFlags ObjectFlags, const FName& Name ) override;
};
