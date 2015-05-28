// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "Net/UnitTestNetConnection.h"
#include "Net/UnitTestPackageMap.h"


// @todo JohnB: Should probably add a CL-define which wraps this code, since not all codebases will expose PackageMapClient?
//				(probably better to just error, and force a code change for that, in that codebase - otherwise not all unit tests work)


/**
 * Default constructor
 */
UUnitTestPackageMap::UUnitTestPackageMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWithinSerializeNewActor(false)
{
}

bool UUnitTestPackageMap::SerializeObject(FArchive& Ar, UClass* Class, UObject*& Obj, FNetworkGUID* OutNetGUID/*=NULL */)
{
	bool bReturnVal = false;

	bReturnVal = Super::SerializeObject(Ar, Class, Obj, OutNetGUID);

	// This indicates that the new actor channel archetype has just been serialized;
	// this is the first place we know what actor type the actor channel is initializing, but BEFORE it is spawned
	if (GIsInitializingActorChan && bWithinSerializeNewActor && Class == UObject::StaticClass() && Obj != NULL)
	{
		UUnitTestNetConnection* UnitConn = Cast<UUnitTestNetConnection>(GActiveReceiveUnitConnection);
		bool bAllowActorChan = true;

		if (UnitConn != NULL && UnitConn->ActorChannelSpawnDel.IsBound())
		{
			bAllowActorChan = UnitConn->ActorChannelSpawnDel.Execute(Obj->GetClass());
		}

		if (!bAllowActorChan)
		{
			Obj = NULL;

			// NULL the control channel, to break code that would disconnect the client (control chan is recovered, in ReceivedBunch)
			Connection->Channels[0] = NULL;
		}
	}

	return bReturnVal;
}

bool UUnitTestPackageMap::SerializeNewActor(FArchive& Ar, class UActorChannel* Channel, class AActor*& Actor)
{
	bool bReturnVal = false;

	bWithinSerializeNewActor = true;

	bReturnVal = Super::SerializeNewActor(Ar, Channel, Actor);

	bWithinSerializeNewActor = false;

	return bReturnVal;
}

