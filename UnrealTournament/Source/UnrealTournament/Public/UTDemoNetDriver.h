// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Runtime/Engine/Classes/Engine/DemoNetDriver.h"
#include "UTDemoRecSpectator.h"

#include "UTDemoNetDriver.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDemoNetDriver : public UDemoNetDriver
{
	GENERATED_BODY()
public:
	virtual void ProcessRemoteFunction(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject) override
	{
		if (IsServer() && ClientConnections.Num() > 0 && Cast<AUTDemoRecSpectator>(Actor) != NULL)
		{
			InternalProcessRemoteFunction(Actor, SubObject, ClientConnections[0], Function, Parameters, OutParms, Stack, true);
		}
		else
		{
			Super::ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, SubObject);
		}
	}

protected:
	virtual void WriteGameSpecificDemoHeader(TArray<FString>& GameSpecificData) override;
	virtual bool ProcessGameSpecificDemoHeader(const TArray<FString>& GameSpecificData, FString& Error) override;
};