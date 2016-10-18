// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGauntletFlag.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlagDispenser.h"
#include "Net/UnrealNetwork.h"

AUTGauntletFlagDispenser::AUTGauntletFlagDispenser(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

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

	static ConstructorHelpers::FObjectFinder<UParticleSystem> TimerPS(TEXT("ParticleSystem'/Game/RestrictedAssets/Weapons/Weapon_Base_Effects/Particles/P_Weapon_timer_01b_FlagRun.P_Weapon_timer_01b_FlagRun'"));
	TimerEffect = ObjectInitializer.CreateDefaultSubobject<UParticleSystemComponent>(this, TEXT("TimerEffect"));
	if (TimerEffect != NULL)
	{
		TimerEffect->SetTemplate(TimerPS.Object);
		TimerEffect->SetHiddenInGame(false);
		TimerEffect->SetupAttachment(RootComponent);
		TimerEffect->LDMaxDrawDistance = 4000.0f;
		TimerEffect->RelativeLocation.Z = 300.0f;
		TimerEffect->Mobility = EComponentMobility::Movable;
		TimerEffect->SetCastShadow(false);
	}

	static ConstructorHelpers::FObjectFinder<UClass> GFClass(TEXT("Blueprint'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/GhostFlag.GhostFlag_C'"));
	GhostFlagClass = GFClass.Object;
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
	OnFlagChanged();
}


void AUTGauntletFlagDispenser::CreateFlag()
{

	if (MyGhostFlag != nullptr) 
	{
		MyGhostFlag->Destroy();
		MyGhostFlag = nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;

	CarriedObject = GetWorld()->SpawnActor<AUTCarriedObject>(CarriedObjectClass, GetActorLocation() + FVector(0.0f, 0.0f, 96.0f), GetActorRotation(), Params);
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
		OnFlagChanged();
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

void AUTGauntletFlagDispenser::InitRound()
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	MyGhostFlag = GetWorld()->SpawnActor<AUTGhostFlag>(GhostFlagClass, GetActorLocation() + FVector(0.0f,0.0f,96.0f), GetActorRotation(), Params);
	if (MyGhostFlag != nullptr)
	{
		MyGhostFlag->GhostMaster.bSuppressTrails = true;
		MyGhostFlag->GhostMaster.TeamNum = 255;
		MyGhostFlag->GhostMaster.bShowTimer = false;
	}
}

void AUTGauntletFlagDispenser::OnFlagChanged()
{
	if (MyFlag == nullptr)
	{
		TimerEffect->SetHiddenInGame(false);
	}
	else
	{
		TimerEffect->SetHiddenInGame(true);
	}
}

void AUTGauntletFlagDispenser::Tick(float DeltaTime)
{
	if (!TimerEffect->bHiddenInGame)
	{
		AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
		if (GauntletGameState)
		{
			TimerEffect->SetFloatParameter(NAME_RespawnTime, 30.0f);
			TimerEffect->SetFloatParameter(NAME_SecondsPerPip, 3.0f);
			TimerEffect->SetFloatParameter(NAME_Progress, 1.0f - (GauntletGameState->RemainingPickupDelay / 30.0f));
		}
	}
}

