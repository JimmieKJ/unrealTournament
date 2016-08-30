// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGauntletFlag.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlagDispenser.h"
#include "Net/UnrealNetwork.h"

AUTGauntletFlagDispenser::AUTGauntletFlagDispenser(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TeamNum = 255;
	
	static ConstructorHelpers::FObjectFinder<UClass> DefaultFlag(TEXT("Blueprint'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/UTGuantletFlag.UTGuantletFlag_C'"));
	CarriedObjectClass = DefaultFlag.Object;
	
	static ConstructorHelpers::FObjectFinder<USoundCue> CaptureSnd(TEXT("SoundCue'/Game/RestrictedAssets/Audio/Gameplay/A_Gameplay_CTF_CaptureSound_Cue.A_Gameplay_CTF_CaptureSound_Cue'"));
	static ConstructorHelpers::FObjectFinder<USoundWave> AlarmSnd(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Gameplay/A_Gameplay_CTF_FlagAlarm01.A_Gameplay_CTF_FlagAlarm01'"));
	static ConstructorHelpers::FObjectFinder<USoundCue> FlagTakenSnd(TEXT("SoundCue'/Game/RestrictedAssets/Audio/Gameplay/A_Gameplay_CTFEnemyFlagTaken_Cue.A_Gameplay_CTFEnemyFlagTaken_Cue'"));
	static ConstructorHelpers::FObjectFinder<USoundWave> FlagReturnedSnd(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Gameplay/A_Gameplay_CTF_FlagReturn01.A_Gameplay_CTF_FlagReturn01'"));

	FlagScoreRewardSound = CaptureSnd.Object;
	FlagTakenSound = AlarmSnd.Object;
	EnemyFlagTakenSound = FlagTakenSnd.Object;
	FlagReturnedSound = FlagReturnedSnd.Object;

}

void AUTGauntletFlagDispenser::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	if (PC->GetPawn())
	{
		float Scale = Canvas->ClipY / 1080.0f;
		FVector2D Size = FVector2D(14,47) * Scale;

		AUTHUD* HUD = Cast<AUTHUD>(PC->MyHUD);
		FVector FlagLoc = GetActorLocation() + FVector(0.0f,0.0f,128.0f);
		FVector ScreenPosition = Canvas->Project(FlagLoc);

		FVector LookVec;
		FRotator LookDir;
		PC->GetPawn()->GetActorEyesViewPoint(LookVec,LookDir);

		if (HUD && FVector::DotProduct(LookDir.Vector().GetSafeNormal(), (FlagLoc - LookVec)) > 0.0f && 
			ScreenPosition.X > 0 && ScreenPosition.X < Canvas->ClipX && 
			ScreenPosition.Y > 0 && ScreenPosition.Y < Canvas->ClipY)
		{
			Canvas->SetDrawColor(255,255,255,255);
			Canvas->DrawTile(HUD->HUDAtlas, ScreenPosition.X - (Size.X * 0.5f), ScreenPosition.Y - Size.Y, Size.X, Size.Y,1009,0,14,47);
		}
	}
}

void AUTGauntletFlagDispenser::CreateCarriedObject()
{
	return;
}

void AUTGauntletFlagDispenser::Reset()
{
	if (MyFlag != nullptr)
	{
		MyFlag->ClearGhostFlags();
		AUTGauntletGameState* GameState = GetWorld()->GetGameState<AUTGauntletGameState>();
		if (GameState && GameState->Flag == MyFlag)
		{
			GameState->Flag = nullptr;
		}
		MyFlag->Destroy();
	}

	MyFlag = nullptr;
}


void AUTGauntletFlagDispenser::CreateFlag()
{
	FActorSpawnParameters Params;
	Params.Owner = this;

	CarriedObject = GetWorld()->SpawnActor<AUTCarriedObject>(CarriedObjectClass, GetActorLocation() + FVector(0, 0, 96), GetActorRotation(), Params);
	AUTGauntletGameState* GameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	if (GameState && Cast<AUTGauntletFlag>(CarriedObject))
	{
		GameState->Flag = Cast<AUTGauntletFlag>(CarriedObject);
	}

	if (CarriedObject != NULL)
	{
		CarriedObject->Init(this);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("%s Could not create an object of type %s"), *GetNameSafe(this), *GetNameSafe(CarriedObjectClass));
	}

	AUTGauntletFlag* Flag = Cast<AUTGauntletFlag>(CarriedObject);
	if (Flag && Flag->GetMesh())
	{
		Flag->GetMesh()->ClothBlendWeight = Flag->ClothBlendHome;
		Flag->bShouldPingFlag = true;
		Flag->bSingleGhostFlag = false;

		Flag->AutoReturnTime = 8.f;
		Flag->bGradualAutoReturn = true;
		Flag->bDisplayHolderTrail = true;
		Flag->bSlowsMovement = false;
		Flag->bSendHomeOnScore = false;
		Flag->SetActorHiddenInGame(false);
		Flag->bAnyoneCanPickup = false;
		Flag->bFriendlyCanPickup = false;
		Flag->bEnemyCanPickup = false;
		Flag->bTeamPickupSendsHome = false;
		Flag->bEnemyPickupSendsHome = false;
		MyFlag = Flag;
	}

}

FText AUTGauntletFlagDispenser::GetHUDStatusMessage(AUTHUD* HUD)
{
	/*
	AUTGauntletGameState* GameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	if (GameState && GameState->HasMatchStarted())
	{
		if (MyFlag == nullptr)
		{
			return FText::AsNumber(GameState->FlagSpawnTimer);
		}
	}
*/
	return FText::GetEmpty();
}


