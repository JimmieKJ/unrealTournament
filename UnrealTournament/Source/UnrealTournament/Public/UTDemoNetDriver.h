// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Runtime/Engine/Classes/Engine/DemoNetDriver.h"

#include "UTDemoNetDriver.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDemoNetDriver : public UNetDriver// UDemoNetDriver
{
	GENERATED_UCLASS_BODY()

	UUTDemoNetDriver(const FObjectInitializer& OI)
	: Super(OI)
	{}

	// evil, evil hack to get around lack of _API on DemoNetDriver
	// don't try this at home!
	inline void HackProcessRemoteFunction(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject)
	//virtual void ProcessRemoteFunction(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject) override
	{
		if (IsServer() && ClientConnections.Num() > 0)
		{
			InternalProcessRemoteFunction(Actor, SubObject, ClientConnections[0], Function, Parameters, OutParms, Stack, true);
			// need to flush right away to avoid dumb assert
			ClientConnections[0]->FlushNet();
		}
	}
};