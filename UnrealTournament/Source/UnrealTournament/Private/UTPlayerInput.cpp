// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPlayerInput.h"

UUTPlayerInput::UUTPlayerInput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

bool UUTPlayerInput::ExecuteCustomBind(FKey Key, EInputEvent EventType)
{
	APlayerController* PC = GetOuterAPlayerController();
	if (PC && PC->PlayerState && PC->PlayerState->bOnlySpectator)
	{
		//	UE_LOG(UT, Warning, TEXT("Key %s"), *Key.GetDisplayName().ToString());
		for (int32 i = 0; i < SpectatorBinds.Num(); i++)
		{
			//		UE_LOG(UT, Warning, TEXT("Check Key %s bind %s"), *FKey(SpectatorBinds[i].KeyName).GetDisplayName().ToString(), *SpectatorBinds[i].Command);
			if (FKey(SpectatorBinds[i].KeyName) == Key && SpectatorBinds[i].EventType == EventType)
			{
				FStringOutputDevice DummyOut;
				if (GetOuterAPlayerController()->Player->Exec(GetOuterAPlayerController()->GetWorld(), *SpectatorBinds[i].Command, DummyOut))
				{
					return true;
				}
			}
		}
	}
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