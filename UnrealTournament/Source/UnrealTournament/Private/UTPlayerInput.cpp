// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPlayerInput.h"

UUTPlayerInput::UUTPlayerInput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
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

void UUTPlayerInput::UTForceRebuildingKeyMaps(const bool bRestoreDefaults)
{
	CustomBinds.Empty();
	if (bRestoreDefaults)
	{
		CustomBinds = GetDefault<UUTPlayerInput>()->CustomBinds;
	}	

	ForceRebuildingKeyMaps(bRestoreDefaults);

}