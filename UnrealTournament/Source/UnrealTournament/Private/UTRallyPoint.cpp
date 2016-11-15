// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFFlag.h"
#include "UTCharacter.h"
#include "Net/UnrealNetwork.h"
#include "UTFlagRunGame.h"
#include "UTFlagRunGameState.h"
#include "UTATypes.h"
#include "UTRallyPoint.h"
#include "UTCTFMajorMessage.h"
#include "UTDefensePoint.h"

AUTRallyPoint::AUTRallyPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(ACharacter::CapsuleComponentName);
	Capsule->SetCollisionProfileName(FName(TEXT("Pickup")));
	Capsule->InitCapsuleSize(192.f, 192.0f);
	//Capsule->bShouldUpdatePhysicsVolume = false;
	Capsule->Mobility = EComponentMobility::Static;
	Capsule->OnComponentBeginOverlap.AddDynamic(this, &AUTRallyPoint::OnOverlapBegin);
	Capsule->OnComponentEndOverlap.AddDynamic(this, &AUTRallyPoint::OnOverlapEnd);
	RootComponent = Capsule;
	bShowAvailableEffect = true;

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));

	if (!IsRunningCommandlet())
	{
		if (ArrowComponent)
		{
			ArrowComponent->ArrowColor = FColor(150, 200, 255);

			ArrowComponent->ArrowSize = 1.0f;
			ArrowComponent->SetupAttachment(Capsule);
			ArrowComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA

	PrimaryActorTick.bCanEverTick = true;
	bCollideWhenPlacing = true;

	RallyReadyDelay = 3.f;
	MinimumRallyTime = 10.f;
	RallyReadyCountdown = RallyReadyDelay;
	ReplicatedCountdown = RallyReadyCountdown;
	bIsEnabled = true;
	RallyOffset = 0;
	RallyPointState = RallyPointStates::Off;
	RallyAvailableDistance = 4000.f;
}

#if WITH_EDITORONLY_DATA
/** Returns ArrowComponent subobject **/
UArrowComponent* AUTRallyPoint::GetArrowComponent() const { return ArrowComponent; }
#endif

void AUTRallyPoint::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTRallyPoint, bIsEnabled);
	DOREPLIFETIME(AUTRallyPoint, bShowAvailableEffect);
	DOREPLIFETIME(AUTRallyPoint, RallyPointState);
	DOREPLIFETIME(AUTRallyPoint, AmbientSound);
	DOREPLIFETIME(AUTRallyPoint, ReplicatedCountdown);
}

void AUTRallyPoint::BeginPlay()
{
	Super::BeginPlay();

	// associate as team locker with team volume I am in
	TArray<UPrimitiveComponent*> OverlappingComponents;
	Capsule->GetOverlappingComponents(OverlappingComponents);
	MyGameVolume = nullptr;
	int32 BestPriority = -1.f;

	for (auto CompIt = OverlappingComponents.CreateIterator(); CompIt; ++CompIt)
	{
		UPrimitiveComponent* OtherComponent = *CompIt;
		if (OtherComponent && OtherComponent->bGenerateOverlapEvents)
		{
			AUTGameVolume* V = Cast<AUTGameVolume>(OtherComponent->GetOwner());
			if (V && V->Priority > BestPriority)
			{
				if (V->IsOverlapInVolume(*Capsule))
				{
					BestPriority = V->Priority;
					MyGameVolume = V;
				}
			}
		}
	}
	if (MyGameVolume)
	{
		MyGameVolume->RallyPoints.AddUnique(this);
	}

	GlowDecalMaterialInstance = GlowDecalMaterial ? UMaterialInstanceDynamic::Create(GlowDecalMaterial, this) : nullptr;
	if (GlowDecalMaterialInstance)
	{
		FRotator DecalRotation = GetActorRotation();
		DecalRotation.Pitch -= 90.f;
		AvailableDecal = UGameplayStatics::SpawnDecalAtLocation(this, GlowDecalMaterialInstance, FVector(192.f, 192.f, 192.f), GetActorLocation() - FVector(0.f, 0.f, 190.f), DecalRotation);

		static FName NAME_Color(TEXT("Color"));
		GlowDecalMaterialInstance->SetVectorParameterValue(NAME_Color, FVector(1.f, 1.f, 1.f));
		static FName NAME_EmissiveBrightness(TEXT("EmissiveBrightness"));
		GlowDecalMaterialInstance->SetScalarParameterValue(NAME_EmissiveBrightness, 5.f);
	}
	OnAvailableEffectChanged();
}

void AUTRallyPoint::GenerateDefensePoints()
{
	// TODO: a bit hacky here as rally points are only used in FR currently, where we want to associate defense points with the capture point
	AUTCTFFlagBase* EnemyBase = nullptr;
	for (TActorIterator<AUTCTFFlagBase> It(GetWorld()); It; ++It)
	{
		if (It->GetTeamNum() == 1)
		{
			EnemyBase = *It;
			break;
		}
	}
	if (EnemyBase != nullptr)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), GetActorLocation() - FVector(0.0f, 0.0f, 500.0f), ECC_Pawn, FCollisionQueryParams(NAME_None, false, this)))
		{
			FRotator SpawnRot = (GetActorLocation() - EnemyBase->GetActorLocation()).Rotation();
			SpawnRot.Pitch = 0;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AUTDefensePoint* NewPoint = GetWorld()->SpawnActor<AUTDefensePoint>(Hit.Location + FVector(0.0f, 0.0f, 50.0f), SpawnRot, Params);
			if (NewPoint != nullptr)
			{
				NewPoint->Objective = EnemyBase;
				NewPoint->BasePriority = 2;
				EnemyBase->DefensePoints.Add(NewPoint);
			}
		}
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Rally point couldn't find flag base!"))
	}
}

void AUTRallyPoint::Reset_Implementation()
{
	bShowAvailableEffect = false;
	RallyPointState = RallyPointStates::Off;
	FlagNearbyChanged(false);
	SetAmbientSound(nullptr, false);
	GetWorldTimerManager().ClearTimer(EndRallyHandle);
}

void AUTRallyPoint::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if ((Role == ROLE_Authority) && bIsEnabled)
	{
		AUTCTFFlag* CharFlag = Cast<AUTCharacter>(OtherActor) ? Cast<AUTCTFFlag>(((AUTCharacter*)OtherActor)->GetCarriedObject()) : nullptr;
		if (CharFlag != NULL)
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS == NULL || (GS->IsMatchInProgress() && !GS->IsMatchIntermission() && !GS->HasMatchEnded()))
			{
				TouchingFC = Cast<AUTCharacter>(OtherActor);
				StartRallyCharging();
			}
		}
	}
}

void AUTRallyPoint::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if ((Role == ROLE_Authority) && bIsEnabled)
	{
		AUTCTFFlag* CharFlag = Cast<AUTCharacter>(OtherActor) ? Cast<AUTCTFFlag>(((AUTCharacter*)OtherActor)->GetCarriedObject()) : nullptr;
		if (CharFlag != NULL)
		{
			EndRallyCharging();
			TouchingFC = nullptr;
		}
	}
}

void AUTRallyPoint::SetRallyPointState(FName NewState)
{
	RallyPointState = NewState;
	if (Role == ROLE_Authority)
	{
		AUTFlagRunGameState* GameState = GetWorld()->GetGameState<AUTFlagRunGameState>();
		if (GameState && (GameState->CurrentRallyPoint == this))
		{
			if ((RallyPointState != RallyPointStates::Powered) && (RallyPointState != RallyPointStates::Charging))
			{
				GameState->CurrentRallyPoint = nullptr;
			}
		}
		else if (GameState && ((RallyPointState == RallyPointStates::Powered) || (RallyPointState == RallyPointStates::Charging)))
		{
			if (GameState->CurrentRallyPoint)
			{
				GameState->CurrentRallyPoint->RallyPoweredTurnOff();
			}
			GameState->CurrentRallyPoint = this;
		}
	}
}

void AUTRallyPoint::StartRallyCharging()
{
	if (RallyPointState == RallyPointStates::Powered)
	{
		return;
	}
	RallyReadyCountdown = RallyReadyDelay;
	ReplicatedCountdown = RallyReadyCountdown;
	SetRallyPointState(RallyPointStates::Charging);
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRallyChargingChanged();
	}
	if ((Role == ROLE_Authority) && TouchingFC && TouchingFC->GetCarriedObject() && TouchingFC->GetCarriedObject()->bCurrentlyPinged && !GetWorldTimerManager().IsTimerActive(EnemyRallyWarningHandle) && (GetWorld()->GetTimeSeconds() - LastEnemyRallyWarning > 10.f))
	{
		GetWorldTimerManager().SetTimer(EnemyRallyWarningHandle, this, &AUTRallyPoint::WarnEnemyRally, 0.5f, false);
	}
}

void AUTRallyPoint::WarnEnemyRally()
{
	if ((GetWorld()->GetTimeSeconds() - LastEnemyRallyWarning < 10.f) || (RallyPointState != RallyPointStates::Charging))
	{
		return;
	}
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	LastEnemyRallyWarning = GetWorld()->GetTimeSeconds();
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerState* UTPS = Cast<AUTPlayerState>((*Iterator)->PlayerState);
		if (UTPS && UTPS->Team && ((UTPS->Team->TeamIndex == 0) != GS->bRedToCap))
		{
			UTPS->AnnounceStatus(StatusMessage::EnemyRally);
			break;
		}
	}
}

void AUTRallyPoint::RallyPoweredTurnOff()
{
	GetWorldTimerManager().ClearTimer(EndRallyHandle);
	RallyReadyCountdown = RallyReadyDelay;
	ReplicatedCountdown = RallyReadyCountdown;
	SetRallyPointState(RallyPointStates::Off);
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRallyChargingChanged();
	}
}

void AUTRallyPoint::RallyPoweredComplete()
{
	// go to either off or start charging again depending on if FC is touching
	TSet<AActor*> Touching;
	Capsule->GetOverlappingActors(Touching);
	AUTCTFFlag* CharFlag = nullptr;
	TouchingFC = nullptr;
	for (AActor* TouchingActor : Touching)
	{
		CharFlag = Cast<AUTCharacter>(TouchingActor) ? Cast<AUTCTFFlag>(((AUTCharacter*)TouchingActor)->GetCarriedObject()) : nullptr;
		if (CharFlag)
		{
			TouchingFC = Cast<AUTCharacter>(TouchingActor);
			break;
		}
	}
	RallyReadyCountdown = RallyReadyDelay;
	ReplicatedCountdown = RallyReadyCountdown;
	FName NextState = (CharFlag != nullptr) ? RallyPointStates::Charging : RallyPointStates::Off;
	SetRallyPointState(NextState);
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRallyChargingChanged();
	}

	if ( (Role == ROLE_Authority) && (NextState == RallyPointStates::Off))
	{
		AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
		if (UTGS != nullptr)
		{
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
				if (PC && PC->UTPlayerState && PC->UTPlayerState->Team && (UTGS->bRedToCap == (PC->UTPlayerState->Team->TeamIndex == 0)))
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 25, nullptr);
				}
			}
		}
	}
}

void AUTRallyPoint::EndRallyCharging()
{
	if (RallyPointState == RallyPointStates::Powered)
	{
		return;
	}
	RallyReadyCountdown = RallyReadyDelay;
	ReplicatedCountdown = RallyReadyCountdown;
	SetRallyPointState(RallyPointStates::Off);
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRallyChargingChanged();
	}
}

void AUTRallyPoint::FlagNearbyChanged(bool bIsNearby)
{
	bShowAvailableEffect = bIsNearby;

	if (Role == ROLE_Authority)
	{
		if (!bIsNearby)
		{
			RallyReadyCountdown = RallyReadyDelay;
			ReplicatedCountdown = RallyReadyCountdown;
			SetRallyPointState(RallyPointStates::Off);
		}
		else if (RallyPointState == RallyPointStates::Off)
		{
			AUTFlagRunGame* FlagRunGame = GetWorld()->GetAuthGameMode<AUTFlagRunGame>();
			AUTCharacter* NearbyFC = FlagRunGame && FlagRunGame->ActiveFlag ? FlagRunGame->ActiveFlag->HoldingPawn : nullptr;
			if (NearbyFC)
			{
				TSet<AActor*> Touching;
				Capsule->GetOverlappingActors(Touching);
				AUTCTFFlag* CharFlag = nullptr;
				TouchingFC = nullptr;
				for (AActor* TouchingActor : Touching)
				{
					CharFlag = Cast<AUTCharacter>(TouchingActor) ? Cast<AUTCTFFlag>(((AUTCharacter*)TouchingActor)->GetCarriedObject()) : nullptr;
					if (CharFlag)
					{
						TouchingFC = Cast<AUTCharacter>(TouchingActor);
						SetRallyPointState(RallyPointStates::Charging);
						break;
					}
				}
			}
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnAvailableEffectChanged();
	}
}

void AUTRallyPoint::OnRallyChargingChanged()
{
	if (RallyPointState == RallyPointStates::Powered)
	{
		SetAmbientSound(FullyPoweredSound, false);
		ChangeAmbientSoundPitch(PoweringUpSound, 1.5f);
		if (RallyEffectPSC == nullptr)
		{
			RallyEffectPSC = UGameplayStatics::SpawnEmitterAtLocation(this, RallyChargingEffect, GetActorLocation() - FVector(0.f, 0.f, Capsule->GetUnscaledCapsuleHalfHeight()), GetActorRotation());
		}
		static FName NAME_MoteColor(TEXT("MoteColor"));
		RallyEffectPSC->SetVectorParameter(NAME_MoteColor, FVector(1.f, 1.f, 0.f));
		static FName NAME_ParticleVelocity(TEXT("ParticleVelocity"));
		RallyEffectPSC->SetVectorParameter(NAME_ParticleVelocity, FVector(0.f, 0.f, 1000.f));
		static FName NAME_SmallParticleVelocity(TEXT("SmallParticleVelocity"));
		RallyEffectPSC->SetVectorParameter(NAME_SmallParticleVelocity, FVector(0.f, 0.f, 1000.f));
	}
	else
	{
		if (RallyEffectPSC != nullptr)
		{
			// clear it
			RallyEffectPSC->ActivateSystem(false);
			RallyEffectPSC->UnregisterComponent();
			RallyEffectPSC = nullptr;
		}
		if (RallyPointState == RallyPointStates::Charging)
		{
			SetAmbientSound(PoweringUpSound, false);
			ChangeAmbientSoundPitch(PoweringUpSound, 0.5f);
			UUTGameplayStatics::UTPlaySound(GetWorld(), FCTouchedSound, this, SRT_All);
			RallyEffectPSC = UGameplayStatics::SpawnEmitterAtLocation(this, RallyChargingEffect, GetActorLocation() - FVector(0.f, 0.f, Capsule->GetUnscaledCapsuleHalfHeight()), GetActorRotation());
			AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
			bHaveGameState = (UTGS != nullptr);
			static FName NAME_MoteColor(TEXT("MoteColor"));
			RallyEffectPSC->SetVectorParameter(NAME_MoteColor, UTGS && UTGS->bRedToCap ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 0.f, 1.f));
			if ((Role == ROLE_Authority) && (UTGS != nullptr))
			{
				AUTFlagRunGame* FlagRunGame = GetWorld()->GetAuthGameMode<AUTFlagRunGame>();
				AUTCharacter* NearbyFC = FlagRunGame && FlagRunGame->ActiveFlag ? FlagRunGame->ActiveFlag->HoldingPawn : nullptr;
				for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
					if (PC && PC->UTPlayerState && (PC->GetPawn() != NearbyFC) && PC->UTPlayerState->Team && (UTGS->bRedToCap == (PC->UTPlayerState->Team->TeamIndex == 0)))
					{
						PC->UTClientPlaySound(FCTouchedSound);
					}
				}
			}
		}
		else
		{
			SetAmbientSound(nullptr, false);
			UUTGameplayStatics::UTPlaySound(GetWorld(), RallyBrokenSound, this, SRT_All);
			if ((Role == ROLE_Authority) && ((RallyReadyCountdown  < 2.f) || !TouchingFC || TouchingFC->IsDead()))
			{
				AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
				bool bNotifyAllPlayers = !TouchingFC || TouchingFC->IsDead() || (TouchingFC->GetCarriedObject() && TouchingFC->GetCarriedObject()->bCurrentlyPinged);
				for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
					if (PC && PC->UTPlayerState && PC->UTPlayerState->Team && (bNotifyAllPlayers || (UTGS->bRedToCap == (PC->UTPlayerState->Team->TeamIndex == 0))))
					{
						PC->UTClientPlaySound(RallyBrokenSound);
					}
				}
			}
		}
	}
	OnAvailableEffectChanged();
}

void AUTRallyPoint::OnAvailableEffectChanged()
{
	if (bIsEnabled)
	{
		if (AvailableEffectPSC == nullptr)
		{
			AvailableEffectPSC = UGameplayStatics::SpawnEmitterAtLocation(this, AvailableEffect, GetActorLocation() - FVector(0.f, 0.f, 64.f), GetActorRotation());
		}
		FVector RingColor(1.f, 1.f, 1.f);
		if (bShowAvailableEffect)
		{
			AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
			bHaveGameState = (UTGS != nullptr);
			if (bHaveGameState)
			{
				RingColor = (UTGS->bRedToCap || !UTGS->HasMatchStarted()) ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 0.f, 1.f);
			}
		}
		else
		{
			SetAmbientSound(PoweringUpSound, true);
		}
		static FName NAME_RingColor(TEXT("RingColor"));
		AvailableEffectPSC->SetVectorParameter(NAME_RingColor, RingColor);
		if (GlowDecalMaterialInstance)
		{
			static FName NAME_Color(TEXT("Color"));
			GlowDecalMaterialInstance->SetVectorParameterValue(NAME_Color, 5.f*RingColor);
			static FName NAME_EmissiveBrightness(TEXT("EmissiveBrightness"));
			GlowDecalMaterialInstance->SetScalarParameterValue(NAME_EmissiveBrightness, 1.f);
		}
	}
}

// flag run game has pointer to active flag, use this to determine distance. base on flag, not carrier
void AUTRallyPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bIsEnabled && (Role == ROLE_Authority))
	{
		AUTFlagRunGame* FlagRunGame = GetWorld()->GetAuthGameMode<AUTFlagRunGame>();
		if (FlagRunGame && FlagRunGame->ActiveFlag && (RallyPointState != RallyPointStates::Powered))
		{
			FVector FlagLocation = FlagRunGame->ActiveFlag->HoldingPawn ? FlagRunGame->ActiveFlag->HoldingPawn->GetActorLocation() : FlagRunGame->ActiveFlag->GetActorLocation();
			bool bFlagIsClose = ((FlagLocation - GetActorLocation()).Size() < RallyAvailableDistance);
			if (bShowAvailableEffect != bFlagIsClose)
			{
				FlagNearbyChanged(bFlagIsClose);
			}
		}
	}
	if (bShowAvailableEffect)
	{
		if (Role == ROLE_Authority)
		{
			if (RallyPointState == RallyPointStates::Charging)
			{
				AUTFlagRunGame* FlagRunGame = GetWorld()->GetAuthGameMode<AUTFlagRunGame>();
				AUTCharacter* NearbyFC = FlagRunGame && FlagRunGame->ActiveFlag ? FlagRunGame->ActiveFlag->HoldingPawn : nullptr;
				if (!NearbyFC)
				{
					EndRallyCharging();
				}
				else
				{
					RallyReadyCountdown -= DeltaTime;
					ReplicatedCountdown = FMath::Max(0, int32(RallyReadyCountdown));
					if (RallyReadyCountdown <= 0.f)
					{
						AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
						if (UTGS != nullptr)
						{
							for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
							{
								AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
								if (PC && PC->UTPlayerState)
								{
									if (!UTGS->OnSameTeam(NearbyFC, PC))
									{
										PC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 28, NearbyFC->PlayerState);
									}
									else if (PC->UTPlayerState->bCanRally)
									{
										PC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 22, NearbyFC->PlayerState);
									}
								}
							}
						}
						UUTGameplayStatics::UTPlaySound(GetWorld(), ReadyToRallySound, this, SRT_All);
						SetRallyPointState(RallyPointStates::Powered);
						RallyStartTime = GetWorld()->GetTimeSeconds();
						GetWorldTimerManager().SetTimer(EndRallyHandle, this, &AUTRallyPoint::RallyPoweredComplete, MinimumRallyTime, false);
						if (GetNetMode() != NM_DedicatedServer)
						{
							OnRallyChargingChanged();
						}
						ChangeAmbientSoundPitch(PoweringUpSound, 1.5f);
					}
					else
					{
						ChangeAmbientSoundPitch(PoweringUpSound, 1.5f - RallyReadyCountdown / RallyReadyDelay);
					}
				}
			}
		}
		else if (!bHaveGameState)
		{
			AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
			bHaveGameState = (UTGS != nullptr);
			if (bHaveGameState && AvailableEffectPSC)
			{
				if (AvailableEffectPSC)
				{
					static FName NAME_RingColor(TEXT("RingColor"));
					AvailableEffectPSC->SetVectorParameter(NAME_RingColor, UTGS->bRedToCap ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 0.f, 1.f));
				}
				if (GlowDecalMaterialInstance)
				{
					static FName NAME_Color(TEXT("Color"));
					GlowDecalMaterialInstance->SetVectorParameterValue(NAME_Color, UTGS && UTGS->bRedToCap ? FVector(5.f, 0.f, 0.f) : FVector(0.f, 0.f, 5.f));
				}
			}
		}
		else if (RallyPointState == RallyPointStates::Charging)
		{
			ChangeAmbientSoundPitch(PoweringUpSound, AmbientSoundPitch + DeltaTime/RallyReadyDelay);
		}
	}
}

void AUTRallyPoint::SetAmbientSound(USoundBase* NewAmbientSound, bool bClear)
{
	if (bClear)
	{
		if (NewAmbientSound == AmbientSound)
		{
			AmbientSound = NULL;
		}
	}
	else
	{
		AmbientSound = NewAmbientSound;
	}
	AmbientSoundUpdated();
}

void AUTRallyPoint::AmbientSoundUpdated()
{
	if (AmbientSound == NULL)
	{
		if (AmbientSoundComp != NULL)
		{
			AmbientSoundComp->Stop();
		}
	}
	else
	{
		if (AmbientSoundComp == NULL)
		{
			AmbientSoundComp = NewObject<UAudioComponent>(this);
			AmbientSoundComp->bAutoDestroy = false;
			AmbientSoundComp->bAutoActivate = false;
			AmbientSoundComp->SetupAttachment(RootComponent);
			AmbientSoundComp->RegisterComponent();
		}
		if (AmbientSoundComp->Sound != AmbientSound)
		{
			// don't attenuate/spatialize sounds made by a local viewtarget
			AmbientSoundComp->bAllowSpatialization = true;
			AmbientSoundComp->SetPitchMultiplier(1.f);

			if (GEngine->GetMainAudioDevice() && !GEngine->GetMainAudioDevice()->IsHRTFEnabledForAll())
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == this)
					{
						AmbientSoundComp->bAllowSpatialization = false;
						break;
					}
				}
			}

			AmbientSoundComp->SetSound(AmbientSound);
		}
		if (!AmbientSoundComp->IsPlaying())
		{
			AmbientSoundComp->Play();
		}
	}
}

void AUTRallyPoint::ChangeAmbientSoundPitch(USoundBase* InAmbientSound, float NewPitch)
{
	if (AmbientSoundComp && AmbientSound && (AmbientSound == InAmbientSound))
	{
		AmbientSoundPitch = NewPitch;
		AmbientSoundPitchUpdated();
	}
}

void AUTRallyPoint::AmbientSoundPitchUpdated()
{
	if (AmbientSoundComp && AmbientSound)
	{
		AmbientSoundComp->SetPitchMultiplier(AmbientSoundPitch);
	}
}

FVector AUTRallyPoint::GetRallyLocation(AUTCharacter* TestChar)
{
	if (TestChar != nullptr)
	{
		RallyOffset += 3;
		int32 OldRallyOffset = RallyOffset;
		for (int32 i = 0; i < 8 - RallyOffset; i++)
		{
			FVector Adjust(0.f);
			FVector NextLocation = GetActorLocation() + 142.f * FVector(FMath::Sin(2.f*PI*RallyOffset*0.125f), FMath::Cos(2.f*PI*RallyOffset*0.125f), 0.f);
			// check if fits at desired location
			if (!GetWorld()->EncroachingBlockingGeometry(TestChar, NextLocation, TestChar->GetActorRotation(), &Adjust))
			{
				return NextLocation;
			}
			RallyOffset++;
		}
		RallyOffset = 0;
		for (int32 i = 0; i < OldRallyOffset; i++)
		{
			FVector Adjust(0.f);
			FVector NextLocation = GetActorLocation() + 142.f * FVector(FMath::Sin(2.f*PI*RallyOffset*0.125f), FMath::Cos(2.f*PI*RallyOffset*0.125f), 0.f);
			// check if fits at desired location
			if (!GetWorld()->EncroachingBlockingGeometry(TestChar, NextLocation, TestChar->GetActorRotation(), &Adjust))
			{
				return NextLocation;
			}
			RallyOffset++;
		}
	}
	return GetActorLocation();
}


FVector AUTRallyPoint::GetAdjustedScreenPosition(UCanvas* Canvas, const FVector& WorldPosition, const FVector& ViewPoint, const FVector& ViewDir, float Dist, float Edge, bool& bDrawEdgeArrow)
{
	FVector Cross = (ViewDir ^ FVector(0.f, 0.f, 1.f)).GetSafeNormal();
	FVector DrawScreenPosition;
	float ExtraPadding = 0.065f * Canvas->ClipX;
	DrawScreenPosition = Canvas->Project(WorldPosition);
	FVector FlagDir = WorldPosition - ViewPoint;
	if ((ViewDir | FlagDir) < 0.f)
	{
		bool bWasLeft = bBeaconWasLeft;
		bDrawEdgeArrow = true;
		DrawScreenPosition.X = bWasLeft ? Edge + ExtraPadding : Canvas->ClipX - Edge - ExtraPadding;
		DrawScreenPosition.Y = 0.5f*Canvas->ClipY;
		DrawScreenPosition.Z = 0.0f;
		return DrawScreenPosition;
	}
	else if ((DrawScreenPosition.X < 0.f) || (DrawScreenPosition.X > Canvas->ClipX))
	{
		bool bLeftOfScreen = (DrawScreenPosition.X < 0.f);
		float OffScreenDistance = bLeftOfScreen ? -1.f*DrawScreenPosition.X : DrawScreenPosition.X - Canvas->ClipX;
		bDrawEdgeArrow = true;
		DrawScreenPosition.X = bLeftOfScreen ? Edge + ExtraPadding : Canvas->ClipX - Edge - ExtraPadding;
		//Y approaches 0.5*Canvas->ClipY as further off screen
		float MaxOffscreenDistance = Canvas->ClipX;
		DrawScreenPosition.Y = 0.4f*Canvas->ClipY + FMath::Clamp((MaxOffscreenDistance - OffScreenDistance) / MaxOffscreenDistance, 0.f, 1.f) * (DrawScreenPosition.Y - 0.6f*Canvas->ClipY);
		DrawScreenPosition.Y = FMath::Clamp(DrawScreenPosition.Y, 0.25f*Canvas->ClipY, 0.75f*Canvas->ClipY);
		bBeaconWasLeft = bLeftOfScreen;
	}
	else
	{
		bBeaconWasLeft = false;
		DrawScreenPosition.X = FMath::Clamp(DrawScreenPosition.X, Edge, Canvas->ClipX - Edge);
		DrawScreenPosition.Y = FMath::Clamp(DrawScreenPosition.Y, Edge, Canvas->ClipY - Edge);
		DrawScreenPosition.Z = 0.0f;
	}
	return DrawScreenPosition;
}


void AUTRallyPoint::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	AUTPlayerState* ViewerPS = PC ? Cast <AUTPlayerState>(PC->PlayerState) : nullptr;
	AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (!ViewerPS || !UTGS || !ViewerPS->Team || (UTGS->bRedToCap != (ViewerPS->Team->TeamIndex == 0)))
	{
		return;
	}

	if (ViewerPS->CarriedObject)
	{
		if (!bShowAvailableEffect)
		{
			return;
		}
		if (UTGS->CurrentRallyPoint && UTGS->CurrentRallyPoint->TouchingFC)
		{
			return;
		}
	}
	else if (RallyPointState != RallyPointStates::Powered)
	{
		return;
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	const bool bIsViewTarget = (PC->GetViewTarget() == this);
	FVector WorldPosition = GetActorLocation();
	if (UTPC != NULL && GS && !GS->IsMatchIntermission() && !GS->HasMatchEnded() && ((FVector::DotProduct(CameraDir, (WorldPosition - CameraPosition)) > 0.0f) || !ViewerPS->CarriedObject)
		 && (UTPC->MyUTHUD == nullptr || !UTPC->MyUTHUD->bShowScores))
	{
		float TextXL, YL;
		float ScaleTime = FMath::Min(1.f, 6.f * GetWorld()->DeltaTimeSeconds);
		float MinTextScale = 0.75f;
		// BeaconTextScale = (1.f - ScaleTime) * BeaconTextScale + ScaleTime * ((bRecentlyRendered && !bFarAway) ? 1.f : 0.75f);
		float Scale = Canvas->ClipX / 1920.f;
		UFont* SmallFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->SmallFont;
		FText RallyText =NSLOCTEXT("UTRallyPoint", "RallyHere", " RALLY HERE! ");
		if ((RallyPointState == RallyPointStates::Powered) && MyGameVolume)
		{
			FFormatNamedArguments Args;
			Args.Add("RallyLoc", MyGameVolume->VolumeName);
			RallyText = FText::Format(NSLOCTEXT("UTRallyPoint", "RallyAtLoc", " RALLY at {RallyLoc} "), Args);
		}
		Canvas->TextSize(SmallFont, RallyText.ToString(), TextXL, YL, Scale, Scale);
		FVector ViewDir = UTPC->GetControlRotation().Vector();
		float Dist = (CameraPosition - GetActorLocation()).Size();
		bool bDrawEdgeArrow = false; 
		FVector ScreenPosition = GetAdjustedScreenPosition(Canvas, WorldPosition, CameraPosition, ViewDir, Dist, 20.f, bDrawEdgeArrow);
		float XPos = ScreenPosition.X - 0.5f*TextXL;
		float YPos = ScreenPosition.Y - YL;
		if (XPos < Canvas->ClipX || XPos + TextXL < 0.0f)
		{
			FLinearColor TeamColor = FLinearColor::Yellow;
			float CenterFade = 1.f;
			float PctFromCenter = (ScreenPosition - FVector(0.5f*Canvas->ClipX, 0.5f*Canvas->ClipY, 0.f)).Size() / Canvas->ClipX;
			CenterFade = CenterFade * FMath::Clamp(10.f*PctFromCenter, 0.15f, 1.f);
			TeamColor.A = 0.2f * CenterFade;
			UTexture* BarTexture = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->HUDAtlas;

			Canvas->SetLinearDrawColor(TeamColor);
			float Border = 2.f*Scale;
			float Height = 0.75*YL + 0.7f * YL;
			Canvas->DrawTile(Canvas->DefaultTexture, XPos - Border, YPos - YL - Border, TextXL + 2.f*Border, Height + 2.f*Border, 0, 0, 1, 1);
			FLinearColor BeaconTextColor = FLinearColor::White;
			BeaconTextColor.A = 0.6f * CenterFade;
			FUTCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos), FMath::TruncToFloat(Canvas->OrgY + YPos - 1.2f*YL)), RallyText, SmallFont, BeaconTextColor, NULL);
			TextItem.Scale = FVector2D(Scale, Scale);
			TextItem.BlendMode = SE_BLEND_Translucent;
			FLinearColor ShadowColor = FLinearColor::Black;
			ShadowColor.A = BeaconTextColor.A;
			TextItem.EnableShadow(ShadowColor);
			TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
			Canvas->DrawItem(TextItem);

			FFormatNamedArguments Args;
			FText NumberText = FText::AsNumber(int32(0.01f*Dist));
			UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
			Canvas->TextSize(TinyFont, TEXT("XX meters"), TextXL, YL, Scale, Scale);
			Args.Add("Dist", NumberText);
			FText DistText = NSLOCTEXT("UTRallyPoint", "DistanceText", "{Dist} meters");
			TextItem.Font = TinyFont;
			TextItem.Text = FText::Format(DistText, Args);
			TextItem.Position.X = ScreenPosition.X - 0.5f*TextXL;
			TextItem.Position.Y += 0.9f*YL;

			Canvas->DrawItem(TextItem);
		}
	}
}

// FIXMESTEVE show distance and triangle


