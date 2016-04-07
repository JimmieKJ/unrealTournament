// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_SCTFStatus.h"
#include "UTSCTFGameState.h"
#include "UTSCTFFlag.h"

UUTHUDWidget_SCTFStatus::UUTHUDWidget_SCTFStatus(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	OffensePrimaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnOffenseA","!! YOU ARE ON OFFENSE !!");
	OffenseSecondaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnOffenseB","Assault the enemy base!");
	DefensePrimaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnDefenseA","!! YOU ARE ON DEFENSE !!");
	DefenseSecondaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnDefenseB","Protect your base from the enemy!");

	FlagDownPrimary						= NSLOCTEXT("SCTFGame","FlagDownA","!! THE FLAG IS DOWN !!");
	FlagDownOffenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagDownOB","You have {0} seconds to get the flag!  Hurry!");
	FlagDownDefenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagDownDB","Defend the flag for {0} seconds to gain control!");	

	FlagHomePrimary						= NSLOCTEXT("SCTFGame","FlagHomeA","!! THE FLAG HAS BEEN RESET !!");
	FlagHomeOffenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagHomeOB","Get the flag and assault the base!");
	FlagHomeDefenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagHomeDB","Get ready to defend your base!");	


}

void UUTHUDWidget_SCTFStatus::InitializeWidget(AUTHUD* Hud)
{
	for (int32 i=0; i < Hud->HudWidgets.Num(); i++)
	{
		if (Hud->HudWidgets[i] && Cast<UUTHUDWidget_CTFFlagStatus>(Hud->HudWidgets[i]))
		{
			Cast<UUTHUDWidget_CTFFlagStatus>(Hud->HudWidgets[i])->bSuppressMessage = true;
		}
	}

	Super::InitializeWidget(Hud);
}

void UUTHUDWidget_SCTFStatus::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);

	AUTSCTFGameState* GGameState = Cast<AUTSCTFGameState>(UTGameState);
	if (GGameState && GGameState->Flag && GGameState->AttackingTeam != 255)
	{
		PrimaryMessage.bHidden = false;
		SecondaryMessage.bHidden = false;
		bool bOnOffense = GGameState->AttackingTeam == UTPlayerOwner->GetTeamNum();

		if ( GGameState->Flag->ObjectState == CarriedObjectState::Held )
		{
			PrimaryMessage.Text = bOnOffense ? OffensePrimaryMessage : DefensePrimaryMessage;
			SecondaryMessage.Text = bOnOffense ? OffenseSecondaryMessage : DefenseSecondaryMessage;
		}
		else if ( GGameState->Flag->ObjectState == CarriedObjectState::Dropped )
		{
			PrimaryMessage.Text = FlagDownPrimary;
			SecondaryMessage.Text = FText::Format((bOnOffense ? FlagDownOffenseSecondaryMessage: FlagDownDefenseSecondaryMessage), FText::AsNumber(GGameState->Flag->SwapTimer)); 
		}
		else
		{
			PrimaryMessage.Text = FlagHomePrimary;
			SecondaryMessage.Text = bOnOffense ? FlagHomeOffenseSecondaryMessage: FlagHomeDefenseSecondaryMessage; 
		}

		float Scale = 0.85 + (0.3f * FMath::Abs<float>((FMath::Sin(InUTHUDOwner->GetWorld()->GetTimeSeconds() * 2))));
		PrimaryMessage.TextScale = Scale;
	}
	else
	{
		PrimaryMessage.bHidden = true;
		SecondaryMessage.bHidden = true;
	}
}