// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactoryLandscape.generated.h"

UCLASS(MinimalAPI, config=Editor)
class UActorFactoryLandscape : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface
	virtual AActor* SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags ObjectFlags, const FName Name ) override;
	//~ End UActorFactory Interface
};
