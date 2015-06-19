// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPlayerInput.h"

UUTPlayerInput::UUTPlayerInput()
: Super()
{
	AccelerationPower = 0;
	Acceleration = 0.00005f;
	AccelerationOffset = 0;
	AccelerationMax = 1;
}

bool UUTPlayerInput::ExecuteCustomBind(FKey Key, EInputEvent EventType)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOuterAPlayerController());
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
					PC->bHasUsedSpectatingBind = true;
					return true;
				}
			}
		}
	}
	bool bGotBind = false;
	for (int32 i = 0; i < CustomBinds.Num(); i++)
	{
		if (FKey(CustomBinds[i].KeyName) == Key && CustomBinds[i].EventType == EventType)
		{
			FStringOutputDevice DummyOut;
			if (GetOuterAPlayerController()->Player->Exec(GetOuterAPlayerController()->GetWorld(), *CustomBinds[i].Command, DummyOut))
			{
				bGotBind = true;
				// special case allow binding multiple weapon switches to the same key
				if (!CustomBinds[i].Command.StartsWith(TEXT("SwitchWeapon")) && !CustomBinds[i].Command.StartsWith("ToggleTranslocator"))
				{
					return true;
				}
				
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