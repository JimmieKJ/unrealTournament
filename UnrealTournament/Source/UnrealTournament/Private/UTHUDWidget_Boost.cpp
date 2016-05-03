// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Boost.h"
#include "UTCarriedObject.h"
#include "UTCTFGameState.h"

UUTHUDWidget_Boost::UUTHUDWidget_Boost(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	IconScale = 1.5f;

	DesignedResolution = 1080.f;
	Position=FVector2D(0.0f,0.0f) * IconScale;
	Size=FVector2D(101.0f,114.0f) * IconScale;
	ScreenPosition=FVector2D(0.9f, 0.955f);
	Origin=FVector2D(0.0f,1.0f);
}

bool UUTHUDWidget_Boost::ShouldDraw_Implementation(bool bShowScores)
{
	if (UTHUDOwner && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState)
	{
		AUTPlayerState* PlayerState = UTHUDOwner->UTPlayerOwner->UTPlayerState;
		if (PlayerState->BoostClass && !PlayerState->bOutOfLives)
		{
			return true;
		}
	}
	return false;
}


void UUTHUDWidget_Boost::Draw_Implementation(float DeltaTime)
{

	if (UTHUDOwner && UTHUDOwner->UTPlayerOwner)
	{
		AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
		ScreenPosition.X = (UTGameState && UTGameState->CanShowBoostMenu(UTHUDOwner->UTPlayerOwner)) ? 0.9f : 0.7f;
		ScreenPosition.Y = (UTGameState && UTGameState->CanShowBoostMenu(UTHUDOwner->UTPlayerOwner)) ? 0.955f : 1.0f;

		Super::Draw_Implementation(DeltaTime);

		if (UTGameState && UTHUDOwner->UTPlayerOwner->UTPlayerState)
		{
			AUTPlayerState* PlayerState = UTHUDOwner->UTPlayerOwner->UTPlayerState;

			if (PlayerState->BoostClass)
			{

				if (PlayerState->GetAvailableCurrency() > LastCurrency)
				{
					int32 Amount = int32( PlayerState->GetAvailableCurrency() - LastCurrency);
					FloatingNumbers.Add(FCurrentChange(Amount));
				}

				LastCurrency = PlayerState->GetAvailableCurrency();

				DrawTexture(UTHUDOwner->HUDAtlas3, 0,0, 101.0f * IconScale, 114.0f * IconScale, 49.0f, 1.0f, 101.0f, 114.0f, 1.0f, FLinearColor(0.0f,0.0f,0.0f,0.3f));
				DrawTexture(UTHUDOwner->HUDAtlas3, 0,0, 101.0f * IconScale, 114.0f * IconScale, 49.0f, 121.0f, 101.0f, 114.0f, 1.0f, FLinearColor::White);

				AUTInventory* Inv = PlayerState->BoostClass->GetDefaultObject<AUTInventory>();
				if (Inv)
				{
					float XScale = (80.0f * IconScale) / Inv->HUDIcon.UL;
					float Height = (80.0f * IconScale) * (Inv->HUDIcon.VL / Inv->HUDIcon.UL);

					DrawTexture(Inv->HUDIcon.Texture, 51.0f * IconScale, 57.0f * IconScale, 80.0f * IconScale, Height, Inv->HUDIcon.U, Inv->HUDIcon.V, Inv->HUDIcon.UL, Inv->HUDIcon.VL, 1.0f, FLinearColor::White, FVector2D(0.5f, 0.5f));
				}							

				FText LabelA = FText::GetEmpty();
				FText LabelB = FText::GetEmpty();
				if (UTGameState->CanShowBoostMenu(UTHUDOwner->UTPlayerOwner))
				{
					UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
					if (InputSettings)
					{
						for (int32 inputIndex = 0; inputIndex < InputSettings->ActionMappings.Num(); ++inputIndex)
						{
							FInputActionKeyMapping& Action = InputSettings->ActionMappings[inputIndex];
							if (Action.ActionName == "ShowBuyMenu")
							{
								LabelB = FText::Format(NSLOCTEXT("SUTBoostWidget","BuyMenu","Press {0} to change..."), Action.Key.GetDisplayName());
								break;
							}
						}
					}
				}
				else
				{
					AUTReplicatedLoadoutInfo* LoadoutInfo = nullptr;
					for (int32 i=0; i < UTGameState->AvailableLoadout.Num();i++)
					{
						if (UTGameState->AvailableLoadout[i]->ItemClass == PlayerState->BoostClass)
						{
							LoadoutInfo = UTGameState->AvailableLoadout[i];
							break;
						}
					}

					if (LoadoutInfo)
					{
						float Strength = FMath::Clamp<float>(PlayerState->GetAvailableCurrency() / LoadoutInfo->CurrentCost, 0.0f, 1.0f);
						if (Strength < 1.0f)
						{
							float NeededPoints = LoadoutInfo->CurrentCost - PlayerState->GetAvailableCurrency();
							LabelA = NSLOCTEXT("SUTBoostWidget","NotReady","Not Ready");
							LabelB = FText::Format(NSLOCTEXT("SUTBoostWidget","NotReadyFormat","{0}pts. needed"), FText::AsNumber(int(NeededPoints)));
						}
						else
						{
							UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
							if (InputSettings)
							{
								for (int32 inputIndex = 0; inputIndex < InputSettings->ActionMappings.Num(); ++inputIndex)
								{
									FInputActionKeyMapping& Action = InputSettings->ActionMappings[inputIndex];
									if (Action.ActionName == "StartActivatePowerup")
									{
										LabelB = FText::Format(NSLOCTEXT("SUTBoostWidget","Use","Press {0} to activate!"), Action.Key.GetDisplayName());
										break;
									}
								}
							}

						}
						float H = 114.0f * Strength;
						DrawTexture(UTHUDOwner->HUDAtlas3, 0,0, 101.0f * IconScale, H * IconScale, 49.0f, 121.0f, 101.0f, H, 1.0f, FLinearColor::Red);
					}
				}

				if (!LabelA.IsEmpty())
				{
					DrawText(LabelA,  51 * IconScale, -32.0f, UTHUDOwner->SmallFont, FLinearColor(0.0f,0.0f,0.0f,1.0f), 1.0f, 1.0f, FLinearColor(0.8f,0.8f,0.8f,1.0f), ETextHorzPos::Center, ETextVertPos::Bottom);
				}

				if (!LabelB.IsEmpty())
				{
					DrawText(LabelB,  51 * IconScale, 0.0f, UTHUDOwner->SmallFont, FLinearColor(0.0f,0.0f,0.0f,1.0f), 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Bottom);
				}

				int32 Idx = 0;
				while (Idx < FloatingNumbers.Num())
				{	
					FloatingNumbers[Idx].Timer -= DeltaTime;
					if (FloatingNumbers[Idx].Timer <= 0.0f)
					{
						FloatingNumbers.RemoveAt(Idx);
					}
					else
					{
						Idx++;
					}
				}


				for (int32 i=0; i < FloatingNumbers.Num(); i++)
				{
					float Perc = FloatingNumbers[i].Timer / FLOATING_NUMBER_ANIN_TIME;
					FVector2D Offset = FVector2D(-200.0f * RenderScale, -100.0f * RenderScale) * Perc;
					DrawText(FText::AsNumber(FloatingNumbers[i].Amount),  Offset.X, Offset.Y, UTHUDOwner->LargeFont, FLinearColor(0.0f,0.0f,0.0f,1.0f), Perc, Perc, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Bottom);
				}

			}
		}
	}

}

