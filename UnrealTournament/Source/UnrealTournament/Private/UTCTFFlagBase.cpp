// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFFlagBase.h"
#include "Net/UnrealNetwork.h"
#include "UTGhostFlag.h"

AUTCTFFlagBase::AUTCTFFlagBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FlagBaseMesh (TEXT("StaticMesh'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/S_Pickups_Base_Flag.S_Pickups_Base_Flag'"));

	Mesh = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("FlagBase"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetStaticMesh(FlagBaseMesh.Object);
	Mesh->SetupAttachment(RootComponent);

	Capsule = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	// overlap Pawns, no other collision
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->InitCapsuleSize(92.f, 134.0f);
	Capsule->OnComponentBeginOverlap.AddDynamic(this, &AUTCTFFlagBase::OnOverlapBegin);
	Capsule->SetupAttachment(RootComponent);

	RoundLivesAdjustment = 0;
	ShowDefenseEffect = 0;
	bScoreOnlyBase = false;
}

void AUTCTFFlagBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTCTFFlagBase, MyFlag);
	DOREPLIFETIME(AUTCTFFlagBase, ShowDefenseEffect);
}

void AUTCTFFlagBase::ClearDefenseEffect()
{
	ShowDefenseEffect = 0;
	Mesh->SetHiddenInGame(false);
	if (DefensePSC)
	{
		DefensePSC->ActivateSystem(false);
		DefensePSC->UnregisterComponent();
		DefensePSC = nullptr;
	}
}

void AUTCTFFlagBase::SpawnDefenseEffect()
{
	ClearDefenseEffect();
	Mesh->SetHiddenInGame(true);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UParticleSystem* DesiredEffect = (TeamNum == 0) ? RedDefenseEffect : BlueDefenseEffect;
		DefensePSC = UGameplayStatics::SpawnEmitterAtLocation(this, DesiredEffect, GetActorLocation() + FVector(0.f, 0.f, 80.f), GetActorRotation());
	}
	ShowDefenseEffect = TeamNum+1;
}

void AUTCTFFlagBase::OnDefenseEffectChanged()
{
	if (ShowDefenseEffect > 0)
	{
		SpawnDefenseEffect();
	}
	else
	{
		ClearDefenseEffect();
	}
}

void AUTCTFFlagBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AUTCharacter* Character = Cast<AUTCharacter>(OtherActor);
	if (Character != NULL && Role == ROLE_Authority)
	{
		AUTCTFFlag* CharFlag = Cast<AUTCTFFlag>(Character->GetCarriedObject());
		if ( CharFlag != NULL && CharFlag != CarriedObject && (CarriedObject == NULL || CarriedObject->ObjectState == CarriedObjectState::Home) && CharFlag->GetTeamNum() != GetTeamNum() &&
			!GetWorld()->LineTraceTestByChannel(OtherActor->GetActorLocation(), Capsule->GetComponentLocation(), ECC_Pawn, FCollisionQueryParams(), WorldResponseParams) )
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS == NULL || (GS->IsMatchInProgress() && !GS->IsMatchIntermission() && !GS->HasMatchEnded()))
			{
				CharFlag->Score(FName(TEXT("FlagCapture")), CharFlag->HoldingPawn, CharFlag->Holder);
				CharFlag->PlayCaptureEffect();
			}
		}
	}
}

void AUTCTFFlagBase::CreateCarriedObject()
{
	if (bScoreOnlyBase)
	{
		SpawnDefenseEffect();
	}
	else
	{
		if (TeamFlagTypes.IsValidIndex(TeamNum) && TeamFlagTypes[TeamNum] != NULL)
		{
			CarriedObjectClass = TeamFlagTypes[TeamNum];
		}

		Super::CreateCarriedObject();

		MyFlag = Cast<AUTCTFFlag>(CarriedObject);
		if (MyFlag && MyFlag->GetMesh())
		{
			MyFlag->GetMesh()->ClothBlendWeight = MyFlag->ClothBlendHome;
		}
	}
}

FName AUTCTFFlagBase::GetFlagState()
{
	if (MyFlag != NULL)
	{
		return MyFlag->ObjectState;
	}

	return CarriedObjectState::Home;
}

void AUTCTFFlagBase::RecallFlag()
{
	if (MyFlag != NULL)
	{
		if (MyFlag->ObjectState != CarriedObjectState::Home)
		{
			bool bGradualAutoReturn = MyFlag->bGradualAutoReturn;
			MyFlag->bGradualAutoReturn = false;
			MyFlag->PastPositions.Empty();
			MyFlag->SendHome();
			MyFlag->bGradualAutoReturn = bGradualAutoReturn;
		}
	}
}

void AUTCTFFlagBase::ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome)
{
	Super::ObjectWasPickedUp(NewHolder, bWasHome);

	if (bWasHome)
	{
		if (!EnemyFlagTakenSound)
		{
			EnemyFlagTakenSound = FlagTakenSound;
		}
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC && ((PC->PlayerState && PC->PlayerState->bOnlySpectator) || (PC->GetTeamNum() == GetTeamNum())))
			{
				PC->UTClientPlaySound(FlagTakenSound);
			}
			else if (PC)
			{
				PC->HearSound(EnemyFlagTakenSound, this, GetActorLocation(), true, false, SAT_None);
			}
		}
	}

	OnObjectWasPickedUp();
}

void AUTCTFFlagBase::OnObjectWasPickedUp_Implementation()
{

}

void AUTCTFFlagBase::ObjectReturnedHome(AUTCharacter* Returner)
{
	Super::ObjectReturnedHome(Returner);

	UUTGameplayStatics::UTPlaySound(GetWorld(), FlagReturnedSound, this);

	if (MyFlag != NULL && MyFlag->ObjectState == CarriedObjectState::Home)
	{
		// check for friendly flag carrier already here waiting to cap
		TArray<AActor*> Overlapping;
		GetOverlappingActors(Overlapping, APawn::StaticClass());
		for (AActor* A : Overlapping)
		{
			OnOverlapBegin(Capsule, A, Cast<UPrimitiveComponent>(A->GetRootComponent()), 0, false, FHitResult(this, Capsule, A->GetActorLocation(), (A->GetActorLocation() - GetActorLocation()).GetSafeNormal()));
		}
	}
}

void AUTCTFFlagBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
	if (GetNetMode() != NM_DedicatedServer && PC != NULL && PC->MyHUD != NULL)
	{
		PC->MyHUD->AddPostRenderedActor(this);
	}
}

void AUTCTFFlagBase::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
}

FText AUTCTFFlagBase::GetHUDStatusMessage(AUTHUD* HUD)
{
	return FText::GetEmpty();
}

void AUTCTFFlagBase::Reset_Implementation()
{
}

void AUTCTFFlagBase::OnFlagChanged()
{
}
