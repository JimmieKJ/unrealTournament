// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTCTFGameState.h"

UUTHUDWidget_CTFFlagStatus::UUTHUDWidget_CTFFlagStatus(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	YouHaveFlagText = NSLOCTEXT("CTFScore","YouHaveFlagText","You have the flag, return to base!");
	EnemyHasFlagText = NSLOCTEXT("CTFScore","EnemyHasFlagText","The enemy has your flag, recover it!");
	BothFlagsText = NSLOCTEXT("CTFScore","BothFlagsText","Hold enemy flag until your flag is returned!");

	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("Font'/Game/RestrictedAssets/Fonts/fntAmbex36.fntAmbex36'"));
	MessageFont = Font.Object;

	ScreenPosition = FVector2D(0.5f, 0.25f);
	Size = FVector2D(10,10);
	Origin = FVector2D(0.5f,0.5f);
	AnimationAlpha = 0.0f;
}

void UUTHUDWidget_CTFFlagStatus::Draw_Implementation(float DeltaTime)
{
	AUTCTFGameState* CGS = UTHUDOwner->GetWorld()->GetGameState<AUTCTFGameState>();
	if (CGS != NULL && CGS->IsMatchInProgress() && UTHUDOwner != NULL && UTHUDOwner->PlayerOwner != NULL)
	{
		AUTPlayerState* OwnerPS = UTHUDOwner->UTPlayerOwner->UTPlayerState;
		if (OwnerPS != NULL && OwnerPS->Team != NULL)
		{
			uint8 MyTeamNum = OwnerPS->GetTeamNum();
			uint8 OtherTeamNum = MyTeamNum == 0 ? 1 : 0;
			FText StatusText = FText::GetEmpty();

			FLinearColor DrawColor = FLinearColor::Yellow;
			if (CGS->GetFlagState(MyTeamNum) != CarriedObjectState::Home)	// My flag is out there
			{
				DrawColor = FLinearColor::Red;
				// Look to see if I have the enemy flag
				if (OwnerPS->CarriedObject != NULL && Cast<AUTCTFFlag>(OwnerPS->CarriedObject) != NULL)
				{
					StatusText = BothFlagsText;
				}
				else
				{
					if (CGS->GetFlagState(MyTeamNum) == CarriedObjectState::Dropped)
					{
						return;
					}
					else
					{
						StatusText = EnemyHasFlagText;
					}
				}
			}
			else if (OwnerPS->CarriedObject != NULL && Cast<AUTCTFFlag>(OwnerPS->CarriedObject) != NULL)
			{
				StatusText = YouHaveFlagText;
			}

			if (!StatusText.IsEmpty())
			{
				AnimationAlpha += (DeltaTime * 3);
		
				float Alpha = FMath::Sin(AnimationAlpha);
				Alpha = FMath::Abs<float>(Alpha);
				float Scale = 0.85 + (0.15 * Alpha);

				DrawText(StatusText, 0,0, MessageFont, FLinearColor::Black, Scale, Scale, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
			}
			else
			{
				AnimationAlpha = 0.0f;
			}

		}
	}	
}