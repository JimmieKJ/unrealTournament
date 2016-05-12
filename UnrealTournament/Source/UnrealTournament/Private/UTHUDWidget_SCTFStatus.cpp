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

void UUTHUDWidget_SCTFStatus::DrawStatusMessage(float DeltaTime)
{
	AUTSCTFGameState* GameState = Cast<AUTSCTFGameState>(UTGameState);
	if (GameState && GameState->Flag && GameState->AttackingTeam != 255 && GameState->Flag->GetTeamNum() != 255)
	{
		PrimaryMessage.bHidden = false;
		SecondaryMessage.bHidden = false;
		bool bOnOffense = GameState->AttackingTeam == UTPlayerOwner->GetTeamNum();

		if ( GameState->Flag->ObjectState == CarriedObjectState::Held )
		{
			PrimaryMessage.Text = bOnOffense ? OffensePrimaryMessage : DefensePrimaryMessage;
			SecondaryMessage.Text = bOnOffense ? OffenseSecondaryMessage : DefenseSecondaryMessage;
		}
		else if ( GameState->Flag->ObjectState == CarriedObjectState::Dropped )
		{
			PrimaryMessage.Text = FlagDownPrimary;
			SecondaryMessage.Text = FText::Format((bOnOffense ? FlagDownOffenseSecondaryMessage: FlagDownDefenseSecondaryMessage), FText::AsNumber(int32(GameState->Flag->SwapTimer))); 
		}
		else
		{
			PrimaryMessage.Text = FlagHomePrimary;
			SecondaryMessage.Text = bOnOffense ? FlagHomeOffenseSecondaryMessage: FlagHomeDefenseSecondaryMessage; 
		}
	}
	else
	{
		PrimaryMessage.bHidden = true;
		SecondaryMessage.bHidden = true;
	}

	RenderObj_Text(PrimaryMessage);
	RenderObj_Text(SecondaryMessage);
}

void UUTHUDWidget_SCTFStatus::DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation)
{
	if (GameState)
	{
		uint8 OwnerTeam = UTHUDOwner->UTPlayerOwner->GetTeamNum();

		TArray<AUTCTFFlag*> Flags;
		GameState->GetImportantFlag(OwnerTeam, Flags);

		TArray<AUTCTFFlagBase*> FlagBases;
		GameState->GetImportantFlagBase(OwnerTeam, FlagBases); 

		bool bDrawBasesInWorld = true;
		bool bDrawFlagSpawn = false;
		// Flag is there and in the world.
		if (Flags.Num() > 0)
		{
			AUTCTFFlag* Flag = Flags[0];
			AUTCTFFlagBase* FlagBase = FlagBases.Num() > 0 ? FlagBases[0] : nullptr;

			if (Flag->ObjectState != CarriedObjectState::Home)
			{
				uint8 TeamNum = Flag->GetTeamNum();
				DrawFlagStatus(GameState, PlayerViewPoint, PlayerViewRotation, TeamNum, FVector2D(0.0f,0.0f), FlagBase, Flag, Flag->Holder);
				DrawFlagWorld(GameState, PlayerViewPoint, PlayerViewRotation, TeamNum, FlagBase, Flag, Flag->Holder);
				if (FlagBase)
				{
					DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, FlagBase->GetTeamNum(), FlagBase, Flag, Flag->Holder);
					bDrawBasesInWorld = false;
				}
			}
		}
		else
		{
			bDrawFlagSpawn = true;
		}
		
		if (FlagBases.Num() > 0 && bDrawBasesInWorld)
		{
			for (int32 i = 0; i < FlagBases.Num(); i++)
			{
				DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, FlagBases[i]->GetTeamNum(), FlagBases[i], nullptr, nullptr);
				AUTSCTFFlagBase* SFlagBase = Cast<AUTSCTFFlagBase>(FlagBases[i]);
				if (bDrawFlagSpawn && !SFlagBase->bScoreBase)
				{
					// Draw the spawn indicator.
					//DrawSpawnIndicator(GameState, PlayerViewPoint, PlayerViewRotation, SFlagBase);
				}
			}
		}
	}
}

void UUTHUDWidget_SCTFStatus::DrawSpawnIndicator(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, AUTSCTFFlagBase* SFlagBase)
{
	bScaleByDesignedResolution = false;

	// Draw the flag / flag base in the world
	float Dist = (SFlagBase->GetActorLocation() - PlayerViewPoint).Size();
	float WorldRenderScale = RenderScale * FMath::Clamp(MaxIconScale - (Dist - ScalingStartDist) / ScalingEndDist, MinIconScale, MaxIconScale);
	FVector ViewDir = PlayerViewRotation.Vector();
	float Edge = CircleTemplate.GetWidth()* WorldRenderScale;
	FVector WorldPosition = SFlagBase->GetActorLocation() + FVector(0.f, 0.f, 256.0f);
	bool bDrawEdgeArrow = false;
	FVector ScreenPosition = GetAdjustedScreenPosition(WorldPosition, PlayerViewPoint, ViewDir, Dist, Edge, bDrawEdgeArrow, 0);

	ScreenPosition.X -= RenderPosition.X;
	ScreenPosition.Y -= RenderPosition.Y;
	float ViewDist = (PlayerViewPoint - WorldPosition).Size();

	FlagIconTemplate.RenderOpacity = 1.0f;
	CircleTemplate.RenderOpacity = 1.0f;
	CircleBorderTemplate.RenderOpacity = 1.0f;

	float InWorldFlagScale = WorldRenderScale * StatusScale;
	//RenderObj_TextureAt(CircleTemplate, ScreenPosition.X, ScreenPosition.Y, CircleTemplate.GetWidth()* InWorldFlagScale, CircleTemplate.GetHeight()* InWorldFlagScale);
	//RenderObj_TextureAt(CircleBorderTemplate, ScreenPosition.X, ScreenPosition.Y, CircleBorderTemplate.GetWidth()* InWorldFlagScale, CircleBorderTemplate.GetHeight()* InWorldFlagScale);

	AUTSCTFGameState* SCTFGameState = Cast<AUTSCTFGameState>(GameState);
	if (SCTFGameState)
	{
		DrawText(FText::AsNumber(SCTFGameState->FlagSpawnTimer), ScreenPosition.X, ScreenPosition.Y, UTHUDOwner->TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.5f*InWorldFlagScale, 0.5f + 0.5f, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), ETextHorzPos::Center, ETextVertPos::Center);
	}
}


FText UUTHUDWidget_SCTFStatus::GetFlagReturnTime(AUTCTFFlag* Flag)
{	
	AUTSCTFGameState* GameState = Cast<AUTSCTFGameState>(UTGameState);
	AUTSCTFFlag* SFlag = Cast<AUTSCTFFlag>(Flag);
	if (GameState && SFlag)
	{
		if (SFlag->ObjectState == CarriedObjectState::Dropped && SFlag->bPendingTeamSwitch)
		{
			return FText::AsNumber(FMath::Max<int32>(int32(GameState->Flag->SwapTimer),1));
		}
		else
		{
			return FText::GetEmpty();
		}
	}

	return Super::GetFlagReturnTime(Flag);
}


bool UUTHUDWidget_SCTFStatus::ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag)
{
	return (Flag->ObjectState == CarriedObjectState::Dropped) || Flag->bCurrentlyPinged || !bIsEnemyFlag;
}
