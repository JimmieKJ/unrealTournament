// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPlayerInput.h"

UUTPlayerInput::UUTPlayerInput(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

bool UUTPlayerInput::ExecuteCustomBind(FKey Key, EInputEvent EventType)
{
	for (int32 i = 0; i < CustomBinds.Num(); i++)
	{
		if (FKey(CustomBinds[i].KeyName) == Key && CustomBinds[i].EventType == EventType)
		{
			FStringOutputDevice DummyOut;
			if (GetOuterAPlayerController()->Player->Exec(GetOuterAPlayerController()->GetWorld(), *CustomBinds[i].Command, DummyOut))
			{
				return true;
			}
		}
	}
	return false;
}