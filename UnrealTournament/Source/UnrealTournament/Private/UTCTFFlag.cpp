// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTCTFGameMessage.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFRewardMessage.h"
#include "UnrealNetwork.h"
#include "UTCTFRoundGame.h"

static FName NAME_Wipe(TEXT("Wipe"));

AUTCTFFlag::AUTCTFFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Collision->InitCapsuleSize(92.f, 134.0f);
	Mesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CTFFlag"));
	GetMesh()->AlwaysLoadOnClient = true;
	GetMesh()->AlwaysLoadOnServer = true;
	GetMesh()->AttachParent = RootComponent;
	GetMesh()->SetAbsolute(false, false, true);

	MeshOffset = FVector(-64.f, 0.f, -48.f);
	HeldOffset = FVector(0.f, 0.f, 0.f);
	HomeBaseOffset = FVector(64.f, 0.f, -8.f);
	HomeBaseRotOffset.Yaw = 0.0f;

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
	bEnemyCanPickup = true;
	PingedDuration = 2.5f;
}

void AUTCTFFlag::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetNetMode() == NM_DedicatedServer || GetCachedScalabilityCVars().DetailMode == 0)
	{
		GetMesh()->bDisableClothSimulation = true;
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

void AUTCTFFlag::ClientUpdateAttachment(bool bNowAttachedToPawn)
{
	if (GetMesh())
	{
		GetMesh()->SetAbsolute(false, false, true);
		if (bNowAttachedToPawn)
		{
			GetMesh()->SetWorldScale3D(FVector(FlagHeldScale));
			GetMesh()->SetRelativeLocation(HeldOffset);
			GetMesh()->ClothBlendWeight = ClothBlendHeld;
		}
		else
		{
			GetMesh()->SetWorldScale3D(FVector(FlagWorldScale));
			GetMesh()->SetRelativeLocation(MeshOffset);
		}
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
			FlagReturnTime = FMath::Clamp(int32(AutoReturnTime + 0.5f), 0, 255);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(SendHomeWithNotifyHandle);
		}
	}
	GetMesh()->ClothBlendWeight = (ObjectState == CarriedObjectState::Held) ? ClothBlendHeld : ClothBlendHome;
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
	GetMesh()->SetRelativeLocation(MeshOffset);
	GetMesh()->ClothBlendWeight = ClothBlendHome;
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
				AUTCTFBaseGame* GM = GetWorld()->GetAuthGameMode<AUTCTFBaseGame>();
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
			GetMesh()->bDisableClothSimulation = true;
			GetMesh()->ClothBlendWeight = ClothBlendHome;
			ReturningMesh = DuplicateObject<USkeletalMeshComponent>(Mesh, this);
			if (GetCachedScalabilityCVars().DetailMode != 0)
			{
				GetMesh()->bDisableClothSimulation = false;
			}
			ReturningMesh->AttachParent = NULL;
			ReturningMesh->RelativeLocation = Mesh->GetComponentLocation();
			ReturningMesh->RelativeRotation = Mesh->GetComponentRotation();
			ReturningMesh->RelativeScale3D = Mesh->GetComponentScale();
			ReturningMeshMID = ReturningMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(1, GetClass()->GetDefaultObject<AUTCTFFlag>()->Mesh->GetMaterial(1));
			ReturningMeshMID->SetScalarParameterValue(NAME_Wipe, ReturnParamCurve->GetFloatValue(0.0f));
			ReturningMesh->RegisterComponent();
		}
		UGameplayStatics::SpawnEmitterAtLocation(this, ReturnSrcEffect, GetActorLocation() + GetRootComponent()->ComponentToWorld.TransformVectorNoScale(GetMesh()->RelativeLocation), GetActorRotation());
		if (HomeBase != NULL)
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, ReturnDestEffect, GetHomeLocation() + FRotationMatrix(GetHomeRotation()).TransformVector(GetMesh()->RelativeLocation), GetHomeRotation());
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
	if (Role == ROLE_Authority)
	{
		bCurrentlyPinged = (GetWorld()->GetTimeSeconds() - LastPingedTime < PingedDuration);
		if ((ObjectState == CarriedObjectState::Held) && (GetWorld()->GetTimeSeconds() - LastPositionUpdateTime > 1.f) && HoldingPawn && HoldingPawn->GetCharacterMovement() && HoldingPawn->GetCharacterMovement()->IsWalking())
		{
			FVector PreviousPos = (PastPositions.Num() > 0) ? PastPositions[PastPositions.Num() - 1] : (HomeBase ? HomeBase->GetActorLocation() : FVector(0.f));
			if ((HoldingPawn->GetActorLocation() - PreviousPos).Size() > 800.f)
			{
				LastPositionUpdateTime = GetWorld()->GetTimeSeconds();
				PastPositions.Add(HoldingPawn->GetActorLocation());
			}
		}
		if ((ObjectState == CarriedObjectState::Dropped) && GetWorldTimerManager().IsTimerActive(SendHomeWithNotifyHandle))
		{
			FlagReturnTime = FMath::Clamp(int32(GetWorldTimerManager().GetTimerRemaining(SendHomeWithNotifyHandle) + 0.5f), 0, 255);
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