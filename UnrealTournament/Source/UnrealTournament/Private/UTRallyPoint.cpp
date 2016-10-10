// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFFlag.h"
#include "UTCharacter.h"
#include "Net/UnrealNetwork.h"
#include "UTFlagRunGameState.h"
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

	RallyReadyDelay = 2.f;
	RallyReadyCountdown = RallyReadyDelay;
}

void AUTRallyPoint::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTRallyPoint, bShowAvailableEffect);
	DOREPLIFETIME(AUTRallyPoint, AmbientSound);
	DOREPLIFETIME(AUTRallyPoint, AmbientSoundPitch);
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
		GlowDecalMaterialInstance->SetVectorParameterValue(NAME_Color, FVector(0.f, 0.f, 0.f));
		static FName NAME_EmissiveBrightness(TEXT("EmissiveBrightness"));
		GlowDecalMaterialInstance->SetScalarParameterValue(NAME_EmissiveBrightness, 0.f);
	}
}

void AUTRallyPoint::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Role == ROLE_Authority)
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
	if (Role == ROLE_Authority)
	{
		AUTCTFFlag* CharFlag = Cast<AUTCharacter>(OtherActor) ? Cast<AUTCTFFlag>(((AUTCharacter*)OtherActor)->GetCarriedObject()) : nullptr;
		if (CharFlag != NULL)
		{
			EndRallyCharging();
		}
	}
}

void AUTRallyPoint::StartRallyCharging()
{
	RallyReadyCountdown = RallyReadyDelay;
	bIsRallyCharging = true;
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRallyChargingChanged();
	}
}

void AUTRallyPoint::EndRallyCharging()
{
	RallyReadyCountdown = RallyReadyDelay;
	bIsRallyCharging = false;
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRallyChargingChanged();
	}
}

void AUTRallyPoint::FlagCarrierInVolume(AUTCharacter* NewFC)
{
	NearbyFC = NewFC;
	bShowAvailableEffect = (NearbyFC != nullptr);
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnAvailableEffectChanged();
	}
}

//RallyChargingEffect
//FCTouchedSound
//RallyBrokenSound
//ReadyToRallySound

void AUTRallyPoint::OnRallyChargingChanged()
{
	// FIXMESTEVE START AND CLEAR AMBIENTSOUND = poweringupsound
	if (RallyEffectPSC != nullptr)
	{
		// clear it
		RallyEffectPSC->ActivateSystem(false);
		RallyEffectPSC->UnregisterComponent();
		RallyEffectPSC = nullptr;
	}
	if (bIsRallyCharging)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), FCTouchedSound, this, SRT_All);
		RallyEffectPSC = UGameplayStatics::SpawnEmitterAtLocation(this, RallyChargingEffect, GetActorLocation(), GetActorRotation());
		AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
		bHaveGameState = (UTGS != nullptr);
		static FName NAME_Color(TEXT("Color"));
		RallyEffectPSC->SetVectorParameter(NAME_Color, UTGS && UTGS->bRedToCap ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 0.f, 1.f));
	}
	else
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), RallyBrokenSound, this, SRT_All);
	}
}

void AUTRallyPoint::OnAvailableEffectChanged()
{
	// FIXMESTEVE START AND CLEAR AMBIENTSOUND = poweringupsound
	if (AvailableEffectPSC != nullptr)
	{
		// clear it
		AvailableEffectPSC->ActivateSystem(false);
		AvailableEffectPSC->UnregisterComponent();
		AvailableEffectPSC = nullptr;
	}
	if (bShowAvailableEffect)
	{
		AvailableEffectPSC = UGameplayStatics::SpawnEmitterAtLocation(this, AvailableEffect, GetActorLocation() - FVector(0.f, 0.f, 64.f), GetActorRotation());
		AUTFlagRunGameState* UTGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
		bHaveGameState = (UTGS != nullptr);
		static FName NAME_RingColor(TEXT("RingColor"));
		AvailableEffectPSC->SetVectorParameter(NAME_RingColor, UTGS && UTGS->bRedToCap ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 0.f, 1.f));
		//static FName NAME_RingSize(TEXT("RingSize"));
		//AvailableEffectPSC->SetFloatParameter(NAME_RingSize, 0.2f);
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

	if (bShowAvailableEffect)
	{
		if (Role == ROLE_Authority)
		{
			// FIXMESTEVEambientsound up AmbientSoundPitch
			if (!NearbyFC || NearbyFC->IsPendingKillPending() || !NearbyFC->GetCarriedObject())
			{
				UE_LOG(UT, Warning, TEXT("FAILSAFE CLEAR RALLY POINTS"));
				FlagCarrierInVolume(nullptr);
			}
			else if (bIsRallyCharging)
			{
				// if fc touching, decrement RallyReadyCountdown else reset to 2.f
				RallyReadyCountdown -= DeltaTime;
				if (RallyReadyCountdown < 0.f)
				{
					UUTGameplayStatics::UTPlaySound(GetWorld(), ReadyToRallySound, this, SRT_All);
					UGameplayStatics::SpawnEmitterAtLocation(this, RallyReadyEffect, GetActorLocation(), GetActorRotation());
					bIsRallyCharging = false;
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
	}
}

// effect when FC touching scales up to full rally enabled in 2 seconds
// team based effects

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

