// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDemoRecSpectator.h"
#include "UTDemoNetDriver.h"

AUTDemoRecSpectator::AUTDemoRecSpectator(const FObjectInitializer& OI)
: Super(OI)
{
	bShouldPerformFullTickWhenPaused = true;
}

void AUTDemoRecSpectator::ViewPlayerState(APlayerState* PS)
{
	// we have to redirect back to the Pawn because engine hardcoded FTViewTarget code will reject a PlayerState with NULL owner
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (It->IsValid() && It->Get()->PlayerState == PS)
		{
			SetViewTarget(It->Get());
		}
	}
}

void AUTDemoRecSpectator::ViewSelf(FViewTargetTransitionParams TransitionParams)
{
	ServerViewSelf_Implementation(TransitionParams);
}

void AUTDemoRecSpectator::ServerViewProjectileShim()
{
	ServerViewProjectile_Implementation();
}

void AUTDemoRecSpectator::ViewPawn(APawn* PawnToView)
{
	ViewPlayerState(PawnToView->PlayerState);
}

void AUTDemoRecSpectator::ViewAPlayer(int32 dir)
{
	BehindView(bSpectateBehindView);

	APlayerState* const PS = GetNextViewablePlayer(dir);
	if (PlayerState != NULL)
	{
		ViewPlayerState(PS);
	}
}

APlayerState* AUTDemoRecSpectator::GetNextViewablePlayer(int32 dir)
{
	int32 CurrentIndex = -1;
	if (PlayerCameraManager->ViewTarget.GetTargetPawn() != NULL)
	{
		APlayerState* TestPS = PlayerCameraManager->ViewTarget.GetTargetPawn()->PlayerState;
		// Find index of current viewtarget's PlayerState
		for (int32 i = 0; i < GetWorld()->GameState->PlayerArray.Num(); i++)
		{
			if (TestPS == GetWorld()->GameState->PlayerArray[i])
			{
				CurrentIndex = i;
				break;
			}
		}
	}

	// Find next valid viewtarget in appropriate direction
	int32 NewIndex;
	for (NewIndex = CurrentIndex + dir; (NewIndex >= 0) && (NewIndex < GetWorld()->GameState->PlayerArray.Num()); NewIndex = NewIndex + dir)
	{
		APlayerState* const PlayerState = GetWorld()->GameState->PlayerArray[NewIndex];
		if (PlayerState != NULL && !PlayerState->bOnlySpectator)
		{
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				if (It->IsValid() && It->Get()->PlayerState == PlayerState)
				{
					return PlayerState;
				}
			}
		}
	}

	// wrap around
	CurrentIndex = (NewIndex < 0) ? GetWorld()->GameState->PlayerArray.Num() : -1;
	for (NewIndex = CurrentIndex + dir; (NewIndex >= 0) && (NewIndex < GetWorld()->GameState->PlayerArray.Num()); NewIndex = NewIndex + dir)
	{
		APlayerState* const PlayerState = GetWorld()->GameState->PlayerArray[NewIndex];
		if (PlayerState != NULL && !PlayerState->bOnlySpectator)
		{
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				if (It->IsValid() && It->Get()->PlayerState == PlayerState)
				{
					return PlayerState;
				}
			}
		}
	}

	return NULL;
}

void AUTDemoRecSpectator::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	if (GetWorld()->IsServer())
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (Game != NULL)
		{
			HUDClass = Game->HUDClass;
			OnRep_HUDClass();
		}
	}
}

bool AUTDemoRecSpectator::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	// if we're the demo server, record into the demo
	UNetDriver* NetDriver = GetWorld()->DemoNetDriver;
	if (NetDriver != NULL && NetDriver->ServerConnection == NULL)
	{
		NetDriver->ProcessRemoteFunction(this, Function, Parameters, OutParms, Stack, NULL);
		return true;
	}
	else
	{
		return false;
	}
}

void AUTDemoRecSpectator::ClientTravelInternal_Implementation(const FString& URL, ETravelType TravelType, bool bSeamless, FGuid MapPackageGuid)
{
}

void AUTDemoRecSpectator::ClientToggleScoreboard_Implementation(bool bShow)
{
	Super::ClientToggleScoreboard_Implementation(bShow);
}

void AUTDemoRecSpectator::ShowEndGameScoreboard()
{
	Super::ShowEndGameScoreboard();
}

void AUTDemoRecSpectator::ViewFlag(uint8 Index)
{
	//Avoid the RPC and call the implementation directly
	bAutoCam = false;
	ServerViewFlag_Implementation(Index);
}

void AUTDemoRecSpectator::ClientGameEnded_Implementation(AActor* EndGameFocus, bool bIsWinner)
{
	SetViewTarget(EndGameFocus);
	BehindView(true);
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AUTPlayerController::ShowEndGameScoreboard, 3.f, false);
	APlayerController::ClientGameEnded_Implementation(EndGameFocus, bIsWinner);

	TurnOffPawns();
}

void AUTDemoRecSpectator::BeginPlay()
{
	Super::BeginPlay();
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->OpenReplayWindow();
	}
}

void AUTDemoRecSpectator::OnNetCleanup(class UNetConnection* Connection)
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->CloseReplayWindow();
	}
	Super::OnNetCleanup(Connection);
}

void AUTDemoRecSpectator::ToggleReplayWindow()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->ToggleReplayWindow();
	}
}

void AUTDemoRecSpectator::ShowMenu(const FString& Parameters)
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->ShowMenu(Parameters);
	}
}

void AUTDemoRecSpectator::HideMenu()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->HideMenu();
	}
}

void AUTDemoRecSpectator::SmoothTargetViewRotation(APawn* TargetPawn, float DeltaSeconds)
{
	if (!bSpectateBehindView && TargetPawn)
	{
		TargetViewRotation = TargetPawn->GetActorRotation();
		TargetViewRotation.Pitch = TargetPawn->RemoteViewPitch;
		// Decompress remote view pitch from 1 byte
		float ClampedPitch = (TargetPawn->RemoteViewPitch * 360.f / 255.f);
		ClampedPitch = ClampedPitch > 90.f ? ClampedPitch - 360.f : ClampedPitch;
		TargetViewRotation.Pitch = FMath::Clamp<float>(ClampedPitch, -89.f, 89.f);

		struct FBlendHelper
		{
			/** worker function for AUTDemoRecSpectator::SmoothTargetViewRotation() */
			static float BlendRotation(float DeltaTime, float BlendC, float NewC)
			{
				if (FMath::Abs(BlendC - NewC) > 180.f)
				{
					if (BlendC > NewC)
					{
						NewC += 360.f;
					}
					else
					{
						BlendC += 360.f;
					}
				}

				BlendC = (FMath::Abs(BlendC - NewC) > 90.f) ? NewC : BlendC + (NewC - BlendC) * FMath::Min(1.f, 12.f * DeltaTime );
				return FRotator::ClampAxis(BlendC);
			}
		};

		BlendedTargetViewRotation.Pitch = FBlendHelper::BlendRotation(DeltaSeconds, BlendedTargetViewRotation.Pitch, FRotator::ClampAxis(TargetViewRotation.Pitch));
		// yaw is already smoothed when pawn position is replicated  @TODO FIXMESTEVE - UT passes viewpitch as part of movement, so should be able to Lerp just like Yaw
		BlendedTargetViewRotation.Yaw = FRotator::ClampAxis(TargetViewRotation.Yaw);
		BlendedTargetViewRotation.Roll = FBlendHelper::BlendRotation(DeltaSeconds, BlendedTargetViewRotation.Roll, FRotator::ClampAxis(TargetViewRotation.Roll));
		return;
	}

	Super::SmoothTargetViewRotation(TargetPawn, DeltaSeconds);
}

void AUTDemoRecSpectator::InitPlayerState()
{
	Super::InitPlayerState();
	PlayerState->bOnlySpectator = true;
	PlayerState->PlayerName = TEXT("Replay Spectator");

	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	if (UTPS != nullptr)
	{
		UTPS->bIsDemoRecording = true;
	}
}