// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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


void UUTPlayerInput::PostInitProperties()
{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DebugExecBindings.Empty();
#endif
	Super::PostInitProperties();
}

bool UUTPlayerInput::ExecuteCustomBind(FKey Key, EInputEvent EventType)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOuterAPlayerController());
	if (PC && PC->UTPlayerState && (PC->UTPlayerState->bOnlySpectator || PC->UTPlayerState->bOutOfLives)) // PC->IsInState(NAME_Spectating))
	{
		for (int32 i = 0; i < SpectatorBinds.Num(); i++)
		{
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
	bool bGotBind = false;
	for (int32 i = 0; i < CustomBinds.Num(); i++)
	{
		if (FKey(CustomBinds[i].KeyName) == Key && CustomBinds[i].EventType == EventType)
		{
			CustomBinds[i].Command.Replace(TEXT("|"), TEXT(" "));

			FStringOutputDevice DummyOut;
			if (GetOuterAPlayerController()->Player->Exec(GetOuterAPlayerController()->GetWorld(), *CustomBinds[i].Command, DummyOut))
			{
				bGotBind = true;
				// special case allow binding multiple weapon switches to the same key
				if (!CustomBinds[i].Command.StartsWith(TEXT("SwitchWeapon")) && !CustomBinds[i].Command.StartsWith("ToggleTranslocator") && !CustomBinds[i].Command.StartsWith("SelectTranslocator"))
				{
					return true;
				}
				
			}
		}
	}

	if (!bGotBind)
	{
		for (int32 i = 0; i < LocalBinds.Num(); i++)
		{
			if (FKey(LocalBinds[i].KeyName) == Key && LocalBinds[i].EventType == EventType)
			{
				LocalBinds[i].Command.Replace(TEXT("|"), TEXT(" "));

				FStringOutputDevice DummyOut;
				if (GetOuterAPlayerController()->Player->Exec(GetOuterAPlayerController()->GetWorld(), *LocalBinds[i].Command, DummyOut))
				{
					bGotBind = true;
					// special case allow binding multiple weapon switches to the same key
					if (!LocalBinds[i].Command.StartsWith(TEXT("SwitchWeapon")) && !LocalBinds[i].Command.StartsWith("ToggleTranslocator") && !LocalBinds[i].Command.StartsWith("SelectTranslocator"))
					{
						return true;
					}

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