// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealNetwork.h"

#include "UTPersistentReplicatedEmitter.generated.h"

/** persistent emitter that is replicated to clients
* this is used when we need to spawn an effect that can't be easily simulated or triggered on clients through a simple RepNotify
* on dedicated servers, the effect will not play but the Actor should still be spawned and will stay alive long enough to send to current clients\
* the emitter defaults to being based on its Owner, so that the Base can effectively be passed as part of the spawn parameters
*/
UCLASS(Blueprintable, Abstract, Meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTPersistentReplicatedEmitter : public AUTReplicatedEmitter
{
	GENERATED_UCLASS_BODY()
};