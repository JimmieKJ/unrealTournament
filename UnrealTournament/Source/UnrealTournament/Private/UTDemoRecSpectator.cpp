// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDemoRecSpectator.h"
#include "UTDemoNetDriver.h"

AUTDemoRecSpectator::AUTDemoRecSpectator(const FObjectInitializer& OI)
: Super(OI)
{
}

bool AUTDemoRecSpectator::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	// if we're the demo server, record into the demo
	UNetDriver* NetDriver = GetWorld()->DemoNetDriver;
	if (NetDriver != NULL && NetDriver->ServerConnection == NULL)
	{
		// HACK: due to engine issues it's not really a UUTDemoNetDriver and this is an evil hack to access the protected InternalProcessRemoteFunction()
		//NetDriver->ProcessRemoteFunction(this, Function, Parameters, OutParms, Stack, NULL);
		((UUTDemoNetDriver*)NetDriver)->HackProcessRemoteFunction(this, Function, Parameters, OutParms, Stack, NULL);
		return true;
	}
	else
	{
		return false;
	}
}