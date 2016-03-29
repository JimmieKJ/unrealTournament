// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "Net/UnitTestNetConnection.h"
#include "Net/UnitTestPackageMap.h"


/**
 * Default constructor
 */
UUnitTestPackageMap::UUnitTestPackageMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWithinSerializeNewActor(false)
	, bPendingArchetypeSpawn(false)
{
}

bool UUnitTestPackageMap::SerializeObject(FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID* OutNetGUID/*=NULL */)
{
	bool bReturnVal = false;

	bReturnVal = Super::SerializeObject(Ar, InClass, Obj, OutNetGUID);

	if (bWithinSerializeNewActor)
	{
		// This indicates that SerializeObject has failed to find an existing instance when trying to serialize an actor,
		// so it will be spawned clientside later on (after the archetype is serialized) instead.
		// These spawns count as undesired clientside code execution, so filter them through NotifyAllowNetActor.
		if (InClass == AActor::StaticClass() && Obj == NULL)
		{
			bPendingArchetypeSpawn = true;
		}
		// This indicates that a new actor archetype has just been serialized (which may or may not be during actor channel init);
		// this is the first place we know the type of a replicated actor (in an actor channel or otherwise), but BEFORE it is spawned
		else if ((GIsInitializingActorChan || bPendingArchetypeSpawn) && InClass == UObject::StaticClass() && Obj != NULL)
		{
			UUnitTestNetConnection* UnitConn = Cast<UUnitTestNetConnection>(GActiveReceiveUnitConnection);
			bool bAllowActor = false;

			if (UnitConn != NULL && UnitConn->ReplicatedActorSpawnDel.IsBound())
			{
				bAllowActor = UnitConn->ReplicatedActorSpawnDel.Execute(Obj->GetClass(), GIsInitializingActorChan);
			}

			if (!bAllowActor)
			{
				UE_LOG(LogUnitTest, Log,
						TEXT("Blocking replication/spawning of actor on client (add to NotifyAllowNetActor if required)."));

				UE_LOG(LogUnitTest, Log, TEXT("     ActorChannel: %s, Class: %s, Archetype: %s"),
						(GIsInitializingActorChan ? TEXT("true") : TEXT("false")), *Obj->GetClass()->GetFullName(),
						*Obj->GetFullName());

				Obj = NULL;

				// NULL the control channel, to break code that would disconnect the client (control chan is recovered, in ReceivedBunch)
				Connection->Channels[0] = NULL;
			}
		}
	}

	return bReturnVal;
}

bool UUnitTestPackageMap::SerializeNewActor(FArchive& Ar, class UActorChannel* Channel, class AActor*& Actor)
{
	bool bReturnVal = false;

	bWithinSerializeNewActor = true;
	bPendingArchetypeSpawn = false;

	bReturnVal = Super::SerializeNewActor(Ar, Channel, Actor);

	bPendingArchetypeSpawn = false;
	bWithinSerializeNewActor = false;

	// If we are initializing an actor channel, then make this returns false, to block PostNetInit from being called on the actor,
	// which in turn, blocks BeginPlay being called (this can lead to actor component initialization, which can trigger garbage
	// collection issues down the line)
	if (GIsInitializingActorChan)
	{
		bReturnVal = false;
	}

	return bReturnVal;
}

