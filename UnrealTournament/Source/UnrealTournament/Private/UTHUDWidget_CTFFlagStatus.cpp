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

	ScreenPosition = FVector2D(0.5f, 0.6f);

}

void UUTHUDWidget_CTFFlagStatus::Draw_Implementation(float DeltaTime)
{
	AUTCTFGameState* CGS = UTHUDOwner->GetWorld()->GetGameState<AUTCTFGameState>();
	if (CGS != NULL && CGS->IsMatchInProgress() && UTHUDOwner != NULL && UTHUDOwner->PlayerOwner != NULL)
	{
		AUTPlayerState* OwnerPS = UTHUDOwner->UTPlayerOwner->UTPlayerState;
		if (OwnerPS != NULL)
		{
			uint8 MyTeamNum = OwnerPS->GetTeamNum();
			uint8 OtherTeamNum = MyTeamNum == 0 ? 1 : 0;
			FText StatusText = FText::GetEmpty();

			if (CGS->GetFlagState(MyTeamNum) != CarriedObjectState::Home)	// My flag is out there
			{
				// Look to see if I have the enemy flag
				if (OwnerPS->CarriedObject != NULL && Cast<AUTCTFFlag>(OwnerPS->CarriedObject) != NULL)
				{
					StatusText = BothFlagsText;
				}
				else
				{
					StatusText = EnemyHasFlagText;
				}
			}
			else if (OwnerPS->CarriedObject != NULL && Cast<AUTCTFFlag>(OwnerPS->CarriedObject) != NULL)
			{
				StatusText = YouHaveFlagText;
			}

			if (!StatusText.IsEmpty())
			{
				DrawText(StatusText, 0,0, MessageFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
			}
		}
	}	
}