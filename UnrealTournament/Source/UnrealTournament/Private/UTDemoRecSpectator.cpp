// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDemoRecSpectator.h"
#include "UTGameViewportClient.h"
#include "UTDemoNetDriver.h"
#include "UTKillcamPlayback.h"
#include "UTHUD_InstantReplay.h"
#include "UTLocalMessage.h"

DEFINE_LOG_CATEGORY(LogUTDemoRecSpectator);

AUTDemoRecSpectator::AUTDemoRecSpectator(const FObjectInitializer& OI)
	: Super(OI)
{
	bShouldPerformFullTickWhenPaused = true;
}

void AUTDemoRecSpectator::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (QueuedPlayerStateToView)
	{
		ViewPlayerState(QueuedPlayerStateToView);
	}

	if (QueuedViewTargetGuid.IsValid())
	{
		ViewQueuedGuid();
	}
	if (QueuedViewTargetNetId.IsValid())
	{
		ViewQueuedNetId();
	}
}

void AUTDemoRecSpectator::ViewQueuedNetId()
{
	if (GetWorld()->DemoNetDriver == nullptr)
	{
		return;
	}

	APlayerState* PS = nullptr;
	for (int32 i = 0; i < GetWorld()->GetGameState()->PlayerArray.Num(); i++)
	{
		if (GetWorld()->GetGameState()->PlayerArray[i]->UniqueId == QueuedViewTargetNetId)
		{
			PS = GetWorld()->GetGameState()->PlayerArray[i];
			QueuedViewTargetNetId = FUniqueNetIdRepl();
			ViewPlayerState(PS);
			UE_LOG(LogUTDemoRecSpectator, Log, TEXT("Found queued net id!"));
		}
	}
}

void AUTDemoRecSpectator::ViewQueuedGuid()
{
	if (GetWorld()->DemoNetDriver == nullptr)
	{
		return;
	}

	AActor* ActorForGuid = GetWorld()->DemoNetDriver->GetActorForGUID(QueuedViewTargetGuid);
	if (ActorForGuid)
	{
		APawn* KillcamPawn = Cast<APawn>(ActorForGuid);
		if (KillcamPawn && GetViewTarget() != KillcamPawn)
		{
			// If we're kill cam, just try to view this guid forever
			if (!IsKillcamSpectator())
			{
				QueuedViewTargetGuid.Reset();
			}
			ViewPawn(KillcamPawn);
			UE_LOG(LogUTDemoRecSpectator, Log, TEXT("Found queued guid!"));
		}
	}
}

void AUTDemoRecSpectator::ViewPlayerState(APlayerState* PS)
{
	// we have to redirect back to the Pawn because engine hardcoded FTViewTarget code will reject a PlayerState with NULL owner
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (It->IsValid() && It->Get()->PlayerState == PS)
		{
			if (GetViewTarget() != It->Get())
			{
				SetViewTarget(It->Get());
				// If we're kill cam, just try to view this player state forever
				if (!IsKillcamSpectator())
				{
					QueuedPlayerStateToView = nullptr;
				}
				UE_LOG(LogUTDemoRecSpectator, Log, TEXT("Found queued player state!"));
			}
			return;
		}
	}

	UE_CLOG(PS != nullptr, UT, Verbose, TEXT("ViewPlayerState failed to find %s, queuing"), *PS->PlayerName);
	QueuedPlayerStateToView = PS;
}

void AUTDemoRecSpectator::DemoNotifyCausedHit_Implementation(APawn* InstigatorPawn, AUTCharacter* HitPawn, uint8 AppliedDamage, FVector Momentum, const FDamageEvent& DamageEvent)
{
	if (GetViewTarget() == InstigatorPawn)
	{
		ClientNotifyCausedHit(HitPawn, AppliedDamage);
	}
	if (GetViewTarget() == HitPawn)
	{
		APlayerState* InstigatedByState = (InstigatorPawn != NULL) ? InstigatorPawn->PlayerState : NULL;
		FVector RelHitLocation(FVector::ZeroVector);
		FVector ShotDir(FVector::ZeroVector);
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			ShotDir = ((FPointDamageEvent*)&DamageEvent)->ShotDirection;
		}
		else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ((FRadialDamageEvent*)&DamageEvent)->ComponentHits.Num() > 0)
		{
			if (DamageEvent.IsOfType(FUTRadialDamageEvent::ClassID) && (((FUTRadialDamageEvent*)&DamageEvent)->Params.MinimumDamage == ((FUTRadialDamageEvent*)&DamageEvent)->Params.BaseDamage))
			{
				ShotDir = ((FUTRadialDamageEvent*)&DamageEvent)->ShotDirection;
			}
			else
			{
				ShotDir = (((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].ImpactPoint - ((FRadialDamageEvent*)&DamageEvent)->Origin).GetSafeNormal();
			}
		}
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		bool bFriendlyFire = InstigatedByState != PlayerState && GS != NULL && GS->OnSameTeam(InstigatedByState, this);
		ClientNotifyTakeHit(bFriendlyFire, AppliedDamage, FRotator::CompressAxisToByte(ShotDir.Rotation().Yaw));
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
	AGameState* GameState = GetWorld()->GetGameState<AGameState>();
	if (PlayerCameraManager->ViewTarget.GetTargetPawn() != NULL)
	{
		APlayerState* TestPS = PlayerCameraManager->ViewTarget.GetTargetPawn()->PlayerState;
		// Find index of current viewtarget's PlayerState
		for (int32 i = 0; i < GameState->PlayerArray.Num(); i++)
		{
			if (TestPS == GameState->PlayerArray[i])
			{
				CurrentIndex = i;
				break;
			}
		}
	}

	// Find next valid viewtarget in appropriate direction
	int32 NewIndex;
	for (NewIndex = CurrentIndex + dir; (NewIndex >= 0) && (NewIndex < GameState->PlayerArray.Num()); NewIndex = NewIndex + dir)
	{
		APlayerState* const PlayerStateIter = GameState->PlayerArray[NewIndex];
		if (PlayerStateIter != NULL && !PlayerStateIter->bOnlySpectator)
		{
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				if (It->IsValid() && It->Get()->PlayerState == PlayerStateIter)
				{
					return PlayerStateIter;
				}
			}
		}
	}

	// wrap around
	CurrentIndex = (NewIndex < 0) ? GameState->PlayerArray.Num() : -1;
	for (NewIndex = CurrentIndex + dir; (NewIndex >= 0) && (NewIndex < GameState->PlayerArray.Num()); NewIndex = NewIndex + dir)
	{
		APlayerState* const PlayerStateIter = GameState->PlayerArray[NewIndex];
		if (PlayerStateIter != NULL && !PlayerStateIter->bOnlySpectator)
		{
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				if (It->IsValid() && It->Get()->PlayerState == PlayerStateIter)
				{
					return PlayerStateIter;
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
	UUTDemoNetDriver* NetDriver = Cast<UUTDemoNetDriver>(GetWorld()->DemoNetDriver);
	if (NetDriver)
	{
		if (!NetDriver->bIsLocalReplay)
		{
			UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
			if (LocalPlayer)
			{
				LocalPlayer->OpenReplayWindow();
			}
		}
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
	
	UUTDemoNetDriver* NetDriver = Cast<UUTDemoNetDriver>(GetWorld()->DemoNetDriver);
	if (NetDriver)
	{
		if (!NetDriver->bIsLocalReplay)
		{
			PlayerState->bOnlySpectator = true;
			PlayerState->PlayerName = TEXT("Replay Spectator");
		}
	}

	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	if (UTPS != nullptr)
	{
		UTPS->bIsDemoRecording = true;
	}
}


void AUTDemoRecSpectator::SetPlayer(UPlayer* InPlayer)
{
	Super::SetPlayer(InPlayer);

	if (IsKillcamSpectator())
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = this;
		SpawnInfo.Instigator = Instigator;
		SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save HUDs into a map

		MyHUD = GetWorld()->SpawnActor<AHUD>(AUTHUD_InstantReplay::StaticClass(), SpawnInfo);		
	}
}

bool AUTDemoRecSpectator::IsKillcamSpectator() const
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GetLocalPlayer());
	if (LP != nullptr)
	{
		if (LP->GetKillcamPlaybackManager() != nullptr)
		{
			if (LP->GetKillcamPlaybackManager()->GetKillcamWorld() == GetWorld())
			{
				return true;
			}
		}
	}

	return false;
}

void AUTDemoRecSpectator::ViewPlayerNum(int32 Index, uint8 TeamNum)
{
	if (IsKillcamSpectator())
	{
		return;
	}

	Super::ViewPlayerNum(Index, TeamNum);
}

void AUTDemoRecSpectator::EnableAutoCam()
{
	if (IsKillcamSpectator())
	{
		return;
	}

	Super::EnableAutoCam();
}

void AUTDemoRecSpectator::ChooseBestCamera()
{
	if (IsKillcamSpectator())
	{
		return;
	}

	Super::ChooseBestCamera();
}

void AUTDemoRecSpectator::OnAltFire()
{
	if (IsKillcamSpectator())
	{
		return;
	}

	Super::OnAltFire();
}

void AUTDemoRecSpectator::ViewProjectile()
{
	if (IsKillcamSpectator())
	{
		return;
	}

	Super::ViewProjectile();
}

void AUTDemoRecSpectator::DumpSpecInfo()
{
	AActor* CurrentViewTarget = GetViewTarget();
	UE_LOG(LogUTDemoRecSpectator, Log, TEXT("KillCamSpecator: %s"), IsKillcamSpectator() ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogUTDemoRecSpectator, Log, TEXT("ViewTarget: %s"), CurrentViewTarget != nullptr ? *CurrentViewTarget->GetName() : TEXT("No View Target"));
	UE_LOG(LogUTDemoRecSpectator, Log, TEXT("bAutoCam: %s"), bAutoCam ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogUTDemoRecSpectator, Log, TEXT("Camera Mode: %s"), PlayerCameraManager != nullptr ? *PlayerCameraManager->CameraStyle.ToString() : TEXT("No"));

	UE_LOG(LogUTDemoRecSpectator, Log, TEXT("QueuedPlayerStateToView: %s"), QueuedPlayerStateToView != nullptr ? *QueuedPlayerStateToView->PlayerName : TEXT("No"));
	UE_LOG(LogUTDemoRecSpectator, Log, TEXT("QueuedViewTargetGuid: %s"), QueuedViewTargetGuid.IsValid() ? *QueuedViewTargetGuid.ToString() : TEXT("No"));
	UE_LOG(LogUTDemoRecSpectator, Log, TEXT("QueuedViewTargetNetId: %s"), QueuedViewTargetNetId.IsValid() ? *QueuedViewTargetNetId.ToString() : TEXT("No"));
}

void AUTDemoRecSpectator::MulticastReceiveLocalizedMessage_Implementation(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	if (!IsKillcamSpectator())
	{
		return;
	}

	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && DemoDriver->IsFastForwarding())
	{
		return;
	}

	ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

void AUTDemoRecSpectator::ClientReceiveLocalizedMessage_Implementation(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	TSubclassOf<UUTLocalMessage> UTMessage(*Message);
	if (UTMessage && !UTMessage.GetDefaultObject()->bPlayDuringInstantReplay && IsKillcamSpectator())
	{
		return;
	}

	if (GetWorld()->GetNetMode() == NM_Client)
	{
		UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
		if (DemoDriver && DemoDriver->IsServer())
		{
			AUTDemoRecSpectator* DemoRecSpec = Cast<AUTDemoRecSpectator>(DemoDriver->SpectatorController);
			if (DemoRecSpec && (GetWorld()->GetTimeSeconds() - DemoRecSpec->LastKillcamSeekTime) < 1.0f)
			{
				return;
			}
		}
	}

	Super::ClientReceiveLocalizedMessage_Implementation(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}