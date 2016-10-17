// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFFlag.h"
#include "UTCharacter.h"
#include "Net/UnrealNetwork.h"
#include "UTFlagRunGame.h"
#include "UTFlagRunGameState.h"
#include "UTATypes.h"
#include "UTRallyPoint.h"

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

	PrimaryActorTick.bCanEverTick = true;
	bCollideWhenPlacing = true;

	RallyReadyDelay = 3.f;
	MinimumRallyTime = 5.f;
	RallyReadyCountdown = RallyReadyDelay;
	ReplicatedCountdown = RallyReadyCountdown;
	bIsEnabled = true;
	RallyOffset = 0;
}

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

	if (Role == ROLE_Authority)
	{
		AUTFlagRunGame* GameMode = GetWorld()->GetAuthGameMode<AUTFlagRunGame>();
		if (!GameMode || !GameMode->bFixedRally)
		{
			bIsEnabled = false;
			return;
		}
	}
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
		GlowDecalMaterialInstance->SetVectorParameterValue(NAME_Color, FVector(0.f, 0.f, 0.f));
		static FName NAME_EmissiveBrightness(TEXT("EmissiveBrightness"));
		GlowDecalMaterialInstance->SetScalarParameterValue(NAME_EmissiveBrightness, 0.f);
	}
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
}

void AUTRallyPoint::RallyChargingComplete()
{
	// go to either off or start charging again depending on if FC is touching
	TSet<AActor*> Touching;
	Capsule->GetOverlappingActors(Touching);
	AUTCTFFlag* CharFlag = nullptr;
	for (AActor* TouchingActor : Touching)
	{
		CharFlag = Cast<AUTCharacter>(TouchingActor) ? Cast<AUTCTFFlag>(((AUTCharacter*)TouchingActor)->GetCarriedObject()) : nullptr;
		if (CharFlag)
		{
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

void AUTRallyPoint::FlagCarrierInVolume(AUTCharacter* NewFC)
{
	NearbyFC = NewFC;
	bShowAvailableEffect = (NearbyFC != nullptr);

	if (Role == ROLE_Authority)
	{
		if (NearbyFC == nullptr)
		{
			EndRallyCharging();
		}
		else if (RallyPointState == RallyPointStates::Off)
		{
			// go to either off or start charging again depending on if FC is touching
			TSet<AActor*> Touching;
			Capsule->GetOverlappingActors(Touching);
			AUTCTFFlag* CharFlag = nullptr;
			for (AActor* TouchingActor : Touching)
			{
				CharFlag = Cast<AUTCharacter>(TouchingActor) ? Cast<AUTCTFFlag>(((AUTCharacter*)TouchingActor)->GetCarriedObject()) : nullptr;
				if (CharFlag)
				{
					SetRallyPointState(RallyPointStates::Charging);
					break;
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
		SetAmbientSound(PoweringUpSound, false);
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
		}
		else
		{
			SetAmbientSound(PoweringUpSound, true);
			UUTGameplayStatics::UTPlaySound(GetWorld(), RallyBrokenSound, this, SRT_All);
		}
	}
	OnAvailableEffectChanged();
}

void AUTRallyPoint::OnAvailableEffectChanged()
{
	if (AvailableEffectPSC != nullptr)
	{
		// clear it
		AvailableEffectPSC->ActivateSystem(false);
		AvailableEffectPSC->UnregisterComponent();
		AvailableEffectPSC = nullptr;
	}
	if (bShowAvailableEffect && bIsEnabled)
	{
		AvailableEffectPSC = UGameplayStatics::SpawnEmitterAtLocation(this, AvailableEffect, GetActorLocation() - FVector(0.f, 0.f, 64.f), GetActorRotation());
		AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
		bHaveGameState = (UTGS != nullptr);
		FVector RingColor(1.f, 1.f, 0.f);
		if ((RallyPointState != RallyPointStates::Powered) && bHaveGameState)
		{
			RingColor = UTGS->bRedToCap ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 0.f, 1.f);
		}
		static FName NAME_RingColor(TEXT("RingColor"));
		AvailableEffectPSC->SetVectorParameter(NAME_RingColor, RingColor);
		if (GlowDecalMaterialInstance)
		{
			static FName NAME_Color(TEXT("Color"));
			GlowDecalMaterialInstance->SetVectorParameterValue(NAME_Color, UTGS && UTGS->bRedToCap ? FVector(5.f, 0.f, 0.f) : FVector(0.f,0.f,5.f));
			static FName NAME_EmissiveBrightness(TEXT("EmissiveBrightness"));
			GlowDecalMaterialInstance->SetScalarParameterValue(NAME_EmissiveBrightness, 1.f);
		}
	}
	else if (GlowDecalMaterialInstance)
	{
		static FName NAME_Color(TEXT("Color"));
		GlowDecalMaterialInstance->SetVectorParameterValue(NAME_Color, FVector(0.f, 0.f, 0.f));
		static FName NAME_EmissiveBrightness(TEXT("EmissiveBrightness"));
		GlowDecalMaterialInstance->SetScalarParameterValue(NAME_EmissiveBrightness, 0.f);
	}
}

void AUTRallyPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowAvailableEffect && bIsEnabled)
	{
		if (Role == ROLE_Authority)
		{
			if (!NearbyFC || NearbyFC->IsPendingKillPending() || !NearbyFC->GetCarriedObject())
			{
				UE_LOG(UT, Warning, TEXT("FAILSAFE CLEAR RALLY POINTS"));
				FlagCarrierInVolume(nullptr);
			}
			else if (RallyPointState == RallyPointStates::Charging)
			{
				RallyReadyCountdown -= DeltaTime;
				ReplicatedCountdown = FMath::Max(0, int32(RallyReadyCountdown));
				if (RallyReadyCountdown <= 0.f)
				{
					UUTGameplayStatics::UTPlaySound(GetWorld(), ReadyToRallySound, this, SRT_All);
					SetRallyPointState(RallyPointStates::Powered);
					RallyStartTime = GetWorld()->GetTimeSeconds();
					GetWorldTimerManager().SetTimer(EndRallyHandle, this, &AUTRallyPoint::RallyChargingComplete, MinimumRallyTime, false);
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
		RallyOffset++;
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

