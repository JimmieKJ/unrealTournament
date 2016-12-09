// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/PackageMapClient.h"
#include "NetcodeUnitTest.h"
#include "UnitTestPackageMap.generated.h"

class UNetConnection;

/**
 * Package map override, for blocking the creation of actor channels for specific actors (by detecting the actor class being created)
 */
UCLASS(transient)
class UUnitTestPackageMap : public UPackageMapClient
{
	GENERATED_UCLASS_BODY()

#if TARGET_UE4_CL < CL_DEPRECATENEW
	UUnitTestPackageMap(const FObjectInitializer& ObjectInitializer, UNetConnection* InConnection,
						TSharedPtr<FNetGUIDCache> InNetGUIDCache)
		: UPackageMapClient(ObjectInitializer, InConnection, InNetGUIDCache)
		, bWithinSerializeNewActor(false)
		, bPendingArchetypeSpawn(false)
	{
	}
#endif


	virtual bool SerializeObject(FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID* OutNetGUID=NULL) override;

	virtual bool SerializeNewActor(FArchive& Ar, class UActorChannel* Channel, class AActor*& Actor) override;

public:
	/** Whether or not we are currently within execution of SerializeNewActor */
	bool bWithinSerializeNewActor;

	/** Whether or not SerializeNewActor is about to spawn an actor, from an archetype */
	bool bPendingArchetypeSpawn;
};

