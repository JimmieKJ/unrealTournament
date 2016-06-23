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
	Collision->InitCapsuleSize(110.f, 134.0f);
	Mesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CTFFlag"));
	GetMesh()->AlwaysLoadOnClient = true;
	GetMesh()->AlwaysLoadOnServer = true;
	GetMesh()->AttachParent = RootComponent;
	GetMesh()->SetAbsolute(false, false, true);

	MeshOffset = FVector(0, 0.f, -48.f);
	HeldOffset = FVector(0.f, 0.f, 0.f);
	HomeBaseOffset = FVector(0.f, 0.f, -8.f);
	HomeBaseRotOffset.Yaw = 0.0f;

	FlagWorldScale = 1.75f;
	FlagHeldScale = 1.f;
	GetMesh()->SetWorldScale3D(FVector(FlagWorldScale));
	GetMesh()->bEnablePhysicsOnDedicatedServer = false;
	MovementComponent->ProjectileGravityScale=1.3f;
	MovementComponent->bKeepPhysicsVolumeWhenStopped = true;
	MessageClass = UUTCTFGameMessage::StaticClass();
	bAlwaysRelevant = true;
	bTeamPickupSendsHome = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	ClothBlendHome = 0.f;
	ClothBlendHeld = 0.5f;
	bEnemyCanPickup = true;
	PingedDuration = 2.f;
	bShouldPingFlag = false;

	AuraSphere = ObjectInitializer.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, TEXT("AuraSphere"));
	AuraSphere->AttachParent = RootComponent;
	AuraSphere->SetCollisionProfileName(TEXT("NoCollision"));
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
			GetMesh()->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
			GetMesh()->ClothBlendWeight = ClothBlendHeld;
		}
		else
		{
			GetMesh()->SetWorldScale3D(FVector(FlagWorldScale));
			GetMesh()->SetRelativeLocation(MeshOffset);
		}
		if (CustomDepthMesh)
		{
			CustomDepthMesh->SetWorldScale3D(FVector(bNowAttachedToPawn ? FlagHeldScale : FlagWorldScale));
		}
	}
	Super::ClientUpdateAttachment(bNowAttachedToPawn);
}

void AUTCTFFlag::OnObjectStateChanged()
{
	Super::OnObjectStateChanged();

	if (Role == ROLE_Authority)
	{
		if (ObjectState == CarriedObjectState::Dropped)
		{
			GetWorldTimerManager().SetTimer(SendHomeWithNotifyHandle, this, &AUTCTFFlag::SendHomeWithNotify, AutoReturnTime, false);
			FlagReturnTime = FMath::Clamp(int32(AutoReturnTime + 1.f), 0, 255);
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
	if (bGradualAutoReturn)
	{
		if (Role == ROLE_Authority)
		{
			// if team member nearby, wait a bit longer
			AUTCTFGameState* GameState = GetWorld()->GetGameState<AUTCTFGameState>();
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				AUTCharacter* TeamChar = Cast<AUTCharacter>(*It);
				if (TeamChar && !TeamChar->IsDead() && ((GetActorLocation() - TeamChar->GetActorLocation()).SizeSquared() < 810000.f) && GameState && GameState->OnSameTeam(TeamChar, this) 
					&& TeamChar->GetController() && (TeamChar->GetController()->LineOfSightTo(this) || ((GetActorLocation() - TeamChar->GetActorLocation()).SizeSquared() < 160000.f)))
				{
					GetWorldTimerManager().SetTimer(SendHomeWithNotifyHandle, this, &AUTCTFFlag::SendHomeWithNotify, 0.2f, false);
					return;
				}
			}
		}

		AUTCTFFlagBase* FlagBase = Cast<AUTCTFFlagBase>(HomeBase);
		if (FlagBase)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), FlagBase->FlagReturnedSound, this);
		}
	}
	else
	{
		SendGameMessage(1, NULL, NULL);
	}
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
					GM->BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), 6, Killer->PlayerState, Holder, NULL);
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
		LastDroppedMessageTime = GetWorld()->GetTimeSeconds();
	}
	NoLongerHeld();

	if (HomeBase != NULL)
	{
		HomeBase->ObjectWasDropped(LastHoldingPawn);
	}
	ChangeState(CarriedObjectState::Dropped);

	// Toss is out
	TossObject(LastHoldingPawn);

	if (bGradualAutoReturn && (PastPositions.Num() > 0))
	{
		if ((GetActorLocation() - PastPositions[PastPositions.Num() - 1].Location).Size() < MinGradualReturnDist)
		{
			PastPositions.RemoveAt(PastPositions.Num() - 1);
		}
		if (PastPositions.Num() > 0)// fimxesteve why?
		{
			PutGhostFlagAt(PastPositions[PastPositions.Num() - 1]);
		}
	}
}

void AUTCTFFlag::DelayedDropMessage()
{
	if ((LastGameMessageTime < FlagDropTime) && (ObjectState == CarriedObjectState::Dropped))
	{
		SendGameMessage(3, LastHolder, NULL);
		LastDroppedMessageTime = GetWorld()->GetTimeSeconds();
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

void AUTCTFFlag::UpdateOutline()
{
	const bool bOutlined = (GetNetMode() != NM_DedicatedServer) && ((HoldingPawn != nullptr) ? HoldingPawn->IsOutlined() : bGradualAutoReturn);
	// 0 is a null value for the stencil so use team + 1
	// last bit in stencil is a bitflag so empty team uses 127
	uint8 NewStencilValue = (GetTeamNum() == 255) ? 127 : (GetTeamNum() + 1);
	if (HoldingPawn != NULL && HoldingPawn->GetOutlineWhenUnoccluded())
	{
		NewStencilValue |= 128;
	}
	if (bOutlined)
	{
		GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
		if (CustomDepthMesh == NULL)
		{
			CustomDepthMesh = Cast<USkeletalMeshComponent>(CreateCustomDepthOutlineMesh(GetMesh(), this));
			CustomDepthMesh->SetWorldScale3D(FVector(HoldingPawn ? FlagHeldScale : FlagWorldScale));
		}
		if (CustomDepthMesh->CustomDepthStencilValue != NewStencilValue)
		{
			CustomDepthMesh->CustomDepthStencilValue = NewStencilValue;
			CustomDepthMesh->MarkRenderStateDirty();
		}
		if (!CustomDepthMesh->IsRegistered())
		{
			CustomDepthMesh->RegisterComponent();
			CustomDepthMesh->LastRenderTime = GetMesh()->LastRenderTime;
		}
	}
	else
	{
		GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
		if (CustomDepthMesh != NULL && CustomDepthMesh->IsRegistered())
		{
			CustomDepthMesh->UnregisterComponent();
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
		if (Holder)
		{
			//Update currently pinged
			bCurrentlyPinged = (bShouldPingFlag && (GetWorld()->GetTimeSeconds() - LastPingedTime < PingedDuration));

			Holder->bSpecialPlayer = bCurrentlyPinged;
			if (GetNetMode() != NM_DedicatedServer)
			{
				Holder->OnRepSpecialPlayer();
			}
		}
		if ((ObjectState == CarriedObjectState::Held) && HoldingPawn)
		{
			bool bAddedReturnSpot = false;
			FVector PreviousPos = (PastPositions.Num() > 0) ? PastPositions[PastPositions.Num() - 1].Location : (HomeBase ? HomeBase->GetActorLocation() : FVector(0.f));
			if ((GetWorld()->GetTimeSeconds() - LastPositionUpdateTime > 1.f)  && HoldingPawn->GetCharacterMovement() && HoldingPawn->GetCharacterMovement()->IsWalking() && (!HoldingPawn->GetMovementBase() || !MovementBaseUtility::UseRelativeLocation(HoldingPawn->GetMovementBase())))
			{
				if ((HoldingPawn->GetActorLocation() - PreviousPos).Size() > MinGradualReturnDist)
				{
					if (PastPositions.Num() > 0)
					{
						for (int32 i = 0; i < 3; i++)
						{
							PastPositions[PastPositions.Num() - 1].MidPoints[i] = MidPoints[i];
						}
					}
					LastPositionUpdateTime = GetWorld()->GetTimeSeconds();
					FFlagTrailPos NewPosition;
					NewPosition.Location = HoldingPawn->GetActorLocation();
					NewPosition.MidPoints[0] = FVector::ZeroVector;
					PastPositions.Add(NewPosition);
					MidPointPos = 0;
					bAddedReturnSpot = true;
					MidPoints[0] = FVector::ZeroVector;
					MidPoints[1] = FVector::ZeroVector;
					MidPoints[2] = FVector::ZeroVector;
				}
			}
			if ((MidPointPos < 3) && !bAddedReturnSpot)
			{
				static FName NAME_FlagReturnLOS = FName(TEXT("FlagReturnLOS"));
				FCollisionQueryParams CollisionParms(NAME_FlagReturnLOS, true, HoldingPawn);
				FVector TraceEnd = (MidPointPos > 0) ? MidPoints[MidPointPos - 1] : PreviousPos;
				FHitResult Hit;
				bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, HoldingPawn->GetActorLocation(), TraceEnd, COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms);
				if (bHit)
				{
					MidPointPos++;
				}
				else
				{
					MidPoints[MidPointPos] = HoldingPawn->GetActorLocation() - 100.f * HoldingPawn->GetVelocity().GetSafeNormal();
				}
			}
		}
		if ((ObjectState == CarriedObjectState::Dropped) && GetWorldTimerManager().IsTimerActive(SendHomeWithNotifyHandle))
		{
			FlagReturnTime = FMath::Clamp(int32(GetWorldTimerManager().GetTimerRemaining(SendHomeWithNotifyHandle) + 1.f), 0, 255);
		}
	}
	if (GetNetMode() != NM_DedicatedServer && ((ObjectState == CarriedObjectState::Dropped) || (ObjectState == CarriedObjectState::Home)))
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			if (PlayerController && PlayerController->IsLocalPlayerController() && PlayerController->GetViewTarget())
			{
				FVector Dir = GetActorLocation() - PlayerController->GetViewTarget()->GetActorLocation();
				FRotator DesiredRot = Dir.Rotation();
				DesiredRot.Yaw += 90.f;
				DesiredRot.Pitch = 0.f;
				DesiredRot.Roll = 0.f;
				GetMesh()->SetWorldRotation(DesiredRot, false);
				break;
			}
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