// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AllowWindowsPlatformTypes.h" 

#include "CUESDK.h"

#include "HideWindowsPlatformTypes.h"

#include "CorsairRGB.h"

#include "UnrealTournament.h"
#include "UTPlayerController.h"
#include "UTGameState.h"
#include "UTArmor.h"
#include "UTTimedPowerup.h"

DEFINE_LOG_CATEGORY_STATIC(LogUTKBLightShow, Log, All);

ACorsairRGB::ACorsairRGB(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FCorsairRGB::FCorsairRGB()
{
	FrameTimeMinimum = 0.03f;
	DeltaTimeAccumulator = 0;
	bIsFlashingForEnd = false;
	bInitialized = false;
	bCorsairSDKEnabled = false;
	bPlayingIdleColors = false;

	FlashSpeed = 100;
}

IMPLEMENT_MODULE(FCorsairRGB, CorsairRGB)

void FCorsairRGB::StartupModule()
{
	CorsairPerformProtocolHandshake();
	CorsairError LastError = CorsairGetLastError();
	if (LastError != CE_Success)
	{
		if (LastError != CE_ServerNotFound)
		{
			UE_LOG(LogUTKBLightShow, Warning, TEXT("Failed to enable Corsair RGB"));
		}
	}
	else
	{
		if (CorsairRequestControl(CAM_ExclusiveLightingControl))
		{
			bCorsairSDKEnabled = true;
		}
	}
}

void FCorsairRGB::ShutdownModule()
{
	if (bCorsairSDKEnabled)
	{
		CorsairReleaseControl(CAM_ExclusiveLightingControl);
	}
}

void FCorsairRGB::Tick(float DeltaTime)
{
	if (GIsEditor)
	{
		return;
	}
	
	if (!bCorsairSDKEnabled)
	{
		return;
	}

	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (UserSettings && !UserSettings->IsKeyboardLightingEnabled())
	{
		if (bCorsairSDKEnabled)
		{
			CorsairReleaseControl(CAM_ExclusiveLightingControl);
			bCorsairSDKEnabled = false;
		}

		return;
	}

	// Avoid double ticking
	if (LastFrameCounter > 0 && LastFrameCounter == GFrameCounter)
	{
		return;
	}

	LastFrameCounter = GFrameCounter;

	// We may be going 120hz, don't spam the device
	DeltaTimeAccumulator += DeltaTime;
	if (DeltaTimeAccumulator < FrameTimeMinimum)
	{
		return;
	}
	DeltaTimeAccumulator = 0;

	AUTPlayerController* UTPC = nullptr;
	AUTGameState* GS = nullptr;
	const TIndirectArray<FWorldContext>& AllWorlds = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : AllWorlds)
	{
		UWorld* World = Context.World();
		if (World && World->WorldType == EWorldType::Game)
		{
			UTPC = Cast<AUTPlayerController>(GEngine->GetFirstLocalPlayerController(World));
			if (UTPC)
			{
				UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(UTPC->GetLocalPlayer());
				if (UTLP == nullptr || UTLP->IsMenuGame())
				{
					UTPC = nullptr;
					continue;
				}

				GS = World->GetGameState<AUTGameState>();
				break;
			}
		}
	}

	if (!UTPC || !GS)
	{
		UpdateIdleColors(DeltaTime);
		return;
	}

	if (GS->HasMatchEnded())
	{
		UpdateIdleColors(DeltaTime);
		return;
	}
	else if (GS->HasMatchStarted())
	{
		bPlayingIdleColors = false;

		// Spectators and DM players are all on 255
		int32 TeamNum = UTPC->GetTeamNum();
		
		int R = 0;
		int G = 0;
		int B = 0;
		if (TeamNum == 0)
		{
			R = 255;
		}
		else if (TeamNum == 1)
		{
			B = 255;
		}
		
		TArray<CorsairLedColor> ColorsToSend;

		CorsairLedPositions* ledPositions = CorsairGetLedPositions();
		if (ledPositions)
		{
			ColorsToSend.Reserve(ledPositions->numberOfLed * 2);
		}

		TArray<CorsairLedId> UsedKeys;
		if (UTPC->GetUTCharacter() && !UTPC->GetUTCharacter()->IsDead())
		{
			// Find WASD and other variants like ESDF or IJKL
			TArray<FKey> Keys;
			UTPC->ResolveKeybindToFKey(TEXT("MoveForward"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					auto KeyNameAnsi = StringCast<ANSICHAR>(*KeyName);
					CorsairLedId ledId = CorsairGetLedIdForKeyName(KeyNameAnsi.Get()[0]);
					if (ledId != CLI_Invalid)
					{
						ColorsToSend.Add(CorsairLedColor{ ledId, 0, 255, 0 });
						UsedKeys.Add(ledId);
					}
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("MoveBackward"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					auto KeyNameAnsi = StringCast<ANSICHAR>(*KeyName);
					CorsairLedId ledId = CorsairGetLedIdForKeyName(KeyNameAnsi.Get()[0]);
					if (ledId != CLI_Invalid)
					{
						ColorsToSend.Add(CorsairLedColor{ ledId, 0, 255, 0 });
						UsedKeys.Add(ledId);
					}
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("MoveLeft"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					auto KeyNameAnsi = StringCast<ANSICHAR>(*KeyName);
					CorsairLedId ledId = CorsairGetLedIdForKeyName(KeyNameAnsi.Get()[0]);
					if (ledId != CLI_Invalid)
					{
						ColorsToSend.Add(CorsairLedColor{ ledId, 0, 255, 0 });
						UsedKeys.Add(ledId);
					}
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("MoveRight"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					auto KeyNameAnsi = StringCast<ANSICHAR>(*KeyName);
					CorsairLedId ledId = CorsairGetLedIdForKeyName(KeyNameAnsi.Get()[0]);
					if (ledId != CLI_Invalid)
					{
						ColorsToSend.Add(CorsairLedColor{ ledId, 0, 255, 0 });
						UsedKeys.Add(ledId);
					}
				}
			}
			UTPC->ResolveKeybindToFKey(TEXT("TurnRate"), Keys, false, true);
			for (int i = 0; i < Keys.Num(); i++)
			{
				FString KeyName = Keys[i].ToString();
				if (KeyName.Len() == 1)
				{
					auto KeyNameAnsi = StringCast<ANSICHAR>(*KeyName);
					CorsairLedId ledId = CorsairGetLedIdForKeyName(KeyNameAnsi.Get()[0]);
					if (ledId != CLI_Invalid)
					{
						ColorsToSend.Add(CorsairLedColor{ ledId, 0, 255, 0 });
						UsedKeys.Add(ledId);
					}
				}
			}


			bool bFoundWeapon = false;
			for (int32 i = 0; i < 10; i++)
			{
				bFoundWeapon = false;

				int32 CurrentWeaponGroup = -1;
				if (UTPC->GetUTCharacter()->GetWeapon())
				{
					CurrentWeaponGroup = UTPC->GetUTCharacter()->GetWeapon()->DefaultGroup;
				}

				for (TInventoryIterator<AUTWeapon> It(UTPC->GetUTCharacter()); It; ++It)
				{
					AUTWeapon* Weap = *It;
					if (Weap->HasAnyAmmo())
					{
						if (Weap->DefaultGroup == (i + 1))
						{
							bFoundWeapon = true;
							break;
						}
					}
				}
				
				if (bFoundWeapon)
				{
					switch (i)
					{
					case 0:
					default:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 128, 128, 128 });
						break;
					case 1:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 255, 255, 255 });
						break;
					case 2:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 0, 255, 0 });
						break;
					case 3:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 128, 0, 128 });
						break;
					case 4:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 0, 255, 255 });
						break;
					case 5:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 0, 0, 255 });
						break;
					case 6:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 255, 255, 0 });
						break;
					case 7:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 255, 0, 0 });
						break;
					case 8:
						ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 255, 255, 255 });
						break;
					}
				}
				else
				{
					ColorsToSend.Add(CorsairLedColor{ CorsairLedId(CLK_1 + i), 0, 0, 0 });
				}

				UsedKeys.Add(CorsairLedId(CLK_1 + i));
			}
		}

		if (ledPositions)
		{
			for (int i = 0; i < ledPositions->numberOfLed; i++)
			{
				CorsairLedId ledId = ledPositions->pLedPosition[i].ledId;
				if (!UsedKeys.Contains(ledId))
				{
					ColorsToSend.Add(CorsairLedColor{ ledId, R, G, B });
				}
			}

			CorsairSetLedsColorsAsync(ColorsToSend.Num(), ColorsToSend.GetData(), nullptr, nullptr);
		}

	}
	else
	{
		UpdateIdleColors(DeltaTime);
	}

}

void FCorsairRGB::UpdateIdleColors(float DeltaTime)
{
	if (!bPlayingIdleColors)
	{
		TArray<CorsairLedColor> ColorsToSend;

		CorsairLedPositions* ledPositions = CorsairGetLedPositions();
		if (ledPositions)
		{
			ColorsToSend.Reserve(ledPositions->numberOfLed * 2);
			
			for (int i = 0; i < ledPositions->numberOfLed; i++)
			{
				CorsairLedId ledId = ledPositions->pLedPosition[i].ledId;
				ColorsToSend.Add(CorsairLedColor{ ledId, 255, 255, 255 });
			}
		}

		CorsairSetLedsColorsAsync(ColorsToSend.Num(), ColorsToSend.GetData(), nullptr, nullptr);

		bPlayingIdleColors = true;
	}
}