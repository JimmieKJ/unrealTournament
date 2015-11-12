// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTCTFGameMessage.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFRewardMessage.h"
#include "UnrealNetwork.h"

static FName NAME_Wipe(TEXT("Wipe"));

AUTCTFFlag::AUTCTFFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FlagMesh (TEXT("SkeletalMesh'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/S_CTF_Flag_IronGuard.S_CTF_Flag_IronGuard'"));

	Collision->InitCapsuleSize(92.f, 134.0f);
	Mesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CTFFlag"));
	GetMesh()->SetSkeletalMesh(FlagMesh.Object);
	GetMesh()->AlwaysLoadOnClient = true;
	GetMesh()->AlwaysLoadOnServer = true;
	GetMesh()->AttachParent = RootComponent;
	GetMesh()->SetAbsolute(false, false, true);

	FlagWorldScale = 1.75f;
	FlagHeldScale = 1.f;
	GetMesh()->SetWorldScale3D(FVector(FlagWorldScale));
	GetMesh()->bEnablePhysicsOnDedicatedServer = false;
	MovementComponent->ProjectileGravityScale=1.3f;
	MessageClass = UUTCTFGameMessage::StaticClass();
	bAlwaysRelevant = true;
	bTeamPickupSendsHome = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	ClothBlendHome = 0.f;
	ClothBlendHeld = 0.5f;
}

void AUTCTFFlag::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// backwards compatibility; force values on existing instances
	GetMesh()->SetAbsolute(false, false, true);
	GetMesh()->SetWorldRotation(FRotator(0.0f, 0.f, 0.f));
}

void AUTCTFFlag::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetNetMode() == NM_DedicatedServer || GetCachedScalabilityCVars().DetailMode == 0)
	{
		if (GetMesh())
		{
			GetMesh()->bDisableClothSimulation = true;
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		MeshMID = Mesh->CreateAndSetMaterialInstanceDynamic(1);
	}
}

void AUTCTFFlag::PlayCaptureEffect()
{
	if (Role == ROLE_Authority)
	{
		CaptureEffectCount++;
		ForceNetUpdate();
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(this, CaptureEffect, GetActorLocation() - FVector(0.0f, 0.0f, Collision->GetUnscaledCapsuleHalfHeight()), GetActorRotation());
		if (PSC != NULL)
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS != NULL && GS->Teams.IsValidIndex(GetTeamNum()) && GS->Teams[GetTeamNum()] != NULL)
			{
				PSC->SetColorParameter(FName(TEXT("TeamColor")), GS->Teams[GetTeamNum()]->TeamColor);
			}
		}
	}
}

void AUTCTFFlag::DetachFrom(USkeletalMeshComponent* AttachToMesh)
{
	Super::DetachFrom(AttachToMesh);
	if (GetMesh())
	{
		GetMesh()->SetAbsolute(false, false, true);
		GetMesh()->SetWorldScale3D(FVector(FlagWorldScale));
	}
}

void AUTCTFFlag::AttachTo(USkeletalMeshComponent* AttachToMesh)
{
	Super::AttachTo(AttachToMesh);
	if (AttachToMesh && GetMesh())
	{
		GetMesh()->SetAbsolute(false, false, true);
		GetMesh()->SetWorldScale3D(FVector(FlagHeldScale));
		GetMesh()->ClothBlendWeight = ClothBlendHeld;
	}
}

void AUTCTFFlag::OnObjectStateChanged()
{
	Super::OnObjectStateChanged();

	if (Role == ROLE_Authority)
	{
		if (ObjectState == CarriedObjectState::Dropped)
		{
			GetWorldTimerManager().SetTimer(SendHomeWithNotifyHandle, this, &AUTCTFFlag::SendHomeWithNotify, AutoReturnTime, false);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(SendHomeWithNotifyHandle);
		}
	}
	if (GetMesh())
	{
		GetMesh()->ClothBlendWeight = (ObjectState == CarriedObjectState::Held) ? ClothBlendHeld : ClothBlendHome;
	}
}

void AUTCTFFlag::SendHome()
{
	PlayReturnedEffects();
	Super::SendHome();
}

void AUTCTFFlag::SendHomeWithNotify()
{
	SendGameMessage(1, NULL, NULL);
	SendHome();
}

void AUTCTFFlag::MoveToHome()
{
	Super::MoveToHome();
	if (GetMesh())
	{
		GetMesh()->ClothBlendWeight = ClothBlendHome;
	}
}

void AUTCTFFlag::Drop(AController* Killer)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), DropSound, (HoldingPawn != NULL) ? (AActor*)HoldingPawn : (AActor*)this);

	bool bDelayDroppedMessage = false;
	AUTPlayerState* KillerState = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
	if (KillerState && KillerState->Team && (KillerState != Holder))
	{
		// see if this is a last second save
		AUTCTFGameState* GameState = GetWorld()->GetGameState<AUTCTFGameState>();
		if (GameState)
		{
			AUTCTFFlagBase* OtherBase = GameState->FlagBases[1-GetTeamNum()];
			if (OtherBase && (OtherBase->GetFlagState() == CarriedObjectState::Home) && OtherBase->ActorIsNearMe(this))
			{
				AUTCTFGameMode* GM = GetWorld()->GetAuthGameMode<AUTCTFGameMode>();
				if (GM)
				{
					bDelayDroppedMessage = true;
					GM->BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), 0, Killer->PlayerState, Holder, NULL);
					GM->AddDeniedEventToReplay(Killer->PlayerState, Holder, Holder->Team);
				}
			}
		}
	}

	FlagDropTime = GetWorld()->GetTimeSeconds();
	if (bDelayDroppedMessage)
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFFlag::DelayedDropMessage, 0.8f, false);
	}
	else
	{
		SendGameMessage(3, Holder, NULL);
	}
	NoLongerHeld();

	if (HomeBase != NULL)
	{
		HomeBase->ObjectWasDropped(LastHoldingPawn);
	}
	ChangeState(CarriedObjectState::Dropped);

	// Toss is out
	TossObject(LastHoldingPawn);
}

void AUTCTFFlag::DelayedDropMessage()
{
	if ((LastGameMessageTime < FlagDropTime) && (ObjectState == CarriedObjectState::Dropped))
	{
		SendGameMessage(3, LastHolder, NULL);
	}
}

void AUTCTFFlag::PlayReturnedEffects()
{
	if (GetNetMode() != NM_DedicatedServer && ReturningMesh == NULL) // don't play a second set if first is still playing (guards against redundant calls to SendHome(), etc)
	{
		if (ReturnParamCurve != NULL)
		{
			ReturningMesh = DuplicateObject<USkeletalMeshComponent>(Mesh, this);
			ReturningMesh->AttachParent = NULL;
			ReturningMesh->RelativeLocation = Mesh->GetComponentLocation();
			ReturningMesh->RelativeRotation = Mesh->GetComponentRotation();
			ReturningMesh->RelativeScale3D = Mesh->GetComponentScale();
			ReturningMeshMID = ReturningMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(1, GetClass()->GetDefaultObject<AUTCTFFlag>()->Mesh->GetMaterial(1));
			ReturningMeshMID->SetScalarParameterValue(NAME_Wipe, ReturnParamCurve->GetFloatValue(0.0f));
			ReturningMesh->RegisterComponent();
		}
		UGameplayStatics::SpawnEmitterAtLocation(this, ReturnSrcEffect, GetActorLocation(), GetActorRotation());
		if (HomeBase != NULL)
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, ReturnDestEffect, GetHomeLocation(), HomeBase->GetActorRotation());
		}
	}
}

void AUTCTFFlag::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// tick return material effect
	if (ReturningMeshMID != NULL)
	{
		ReturnEffectTime += DeltaTime;
		float MinTime, MaxTime;
		ReturnParamCurve->GetTimeRange(MinTime, MaxTime);
		if (ReturnEffectTime >= MaxTime)
		{
			ReturningMesh->DestroyComponent(false);
			ReturningMesh = NULL;
			ReturningMeshMID = NULL;
			MeshMID->SetScalarParameterValue(NAME_Wipe, 0.0f);
			ReturnEffectTime = 0.0f;
		}
		else
		{
			const float Value = ReturnParamCurve->GetFloatValue(ReturnEffectTime);
			ReturningMeshMID->SetScalarParameterValue(NAME_Wipe, Value);
			MeshMID->SetScalarParameterValue(NAME_Wipe, 1.0f - Value);
		}
	}
}

void AUTCTFFlag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFFlag, CaptureEffectCount);
}

static FName SavedObjectState;

void AUTCTFFlag::PreNetReceive()
{
	Super::PreNetReceive();

	SavedObjectState = ObjectState;
}

void AUTCTFFlag::PostNetReceiveLocationAndRotation()
{
	if (ObjectState != SavedObjectState && ObjectState == CarriedObjectState::Home)
	{
		PlayReturnedEffects();
	}
	Super::PostNetReceiveLocationAndRotation();
}