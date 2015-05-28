// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Character.cpp: ACharacter implementation
	TODO: Put description here
=============================================================================*/

#include "EnginePrivate.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Net/UnrealNetwork.h"
#include "DisplayDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/DamageType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacter, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogAvatar, Log, All);

FName ACharacter::MeshComponentName(TEXT("CharacterMesh0"));
FName ACharacter::CharacterMovementComponentName(TEXT("CharMoveComp"));
FName ACharacter::CapsuleComponentName(TEXT("CollisionCylinder"));

ACharacter::ACharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName ID_Characters;
		FText NAME_Characters;
		FConstructorStatics()
			: ID_Characters(TEXT("Characters"))
			, NAME_Characters(NSLOCTEXT("SpriteCategory", "Characters", "Characters"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	// Character rotation only changes in Yaw, to prevent the capsule from changing orientation.
	// Ask the Controller for the full rotation if desired (ie for aiming).
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(ACharacter::CapsuleComponentName);
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);

	static FName CollisionProfileName(TEXT("Pawn"));
	CapsuleComponent->SetCollisionProfileName(CollisionProfileName);

	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->bShouldUpdatePhysicsVolume = true;
	CapsuleComponent->bCheckAsyncSceneOnMove = false;
	CapsuleComponent->bCanEverAffectNavigation = false;
	CapsuleComponent->bDynamicObstacle = true;
	RootComponent = CapsuleComponent;

	JumpKeyHoldTime = 0.0f;
	JumpMaxHoldTime = 0.0f;

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->ArrowColor = FColor(150, 200, 255);
		ArrowComponent->bTreatAsASprite = true;
		ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Characters;
		ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Characters;
		ArrowComponent->AttachParent = CapsuleComponent;
		ArrowComponent->bIsScreenSizeScaled = true;
	}
#endif // WITH_EDITORONLY_DATA

	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>(ACharacter::CharacterMovementComponentName);
	if (CharacterMovement)
	{
		CharacterMovement->UpdatedComponent = CapsuleComponent;
		CrouchedEyeHeight = CharacterMovement->CrouchedHalfHeight * 0.80f;
	}

	Mesh = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(ACharacter::MeshComponentName);
	if (Mesh)
	{
		Mesh->AlwaysLoadOnClient = true;
		Mesh->AlwaysLoadOnServer = true;
		Mesh->bOwnerNoSee = false;
		Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose;
		Mesh->bCastDynamicShadow = true;
		Mesh->bAffectDynamicIndirectLighting = true;
		Mesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		Mesh->bChartDistanceFactor = true;
		Mesh->AttachParent = CapsuleComponent;
		static FName CollisionProfileName(TEXT("CharacterMesh"));
		Mesh->SetCollisionProfileName(CollisionProfileName);
		Mesh->bGenerateOverlapEvents = false;
		Mesh->bCanEverAffectNavigation = false;
	}
}

void ACharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (!IsPendingKill())
	{
		if (Mesh)
		{
			BaseTranslationOffset = Mesh->RelativeLocation;

			// force animation tick after movement component updates
			if (Mesh->PrimaryComponentTick.bCanEverTick && CharacterMovement)
			{
				Mesh->PrimaryComponentTick.AddPrerequisite(CharacterMovement, CharacterMovement->PrimaryComponentTick);
			}
		}

		if (CharacterMovement && CapsuleComponent)
		{
			CharacterMovement->UpdateNavAgent(*CapsuleComponent);
		}

		if (Controller == NULL && GetNetMode() != NM_Client)
		{
			if (CharacterMovement && CharacterMovement->bRunPhysicsWithNoController)
			{
				CharacterMovement->SetDefaultMovementMode();
			}
		}
	}
}

UPawnMovementComponent* ACharacter::GetMovementComponent() const
{
	return CharacterMovement;
}


void ACharacter::SetupPlayerInputComponent(UInputComponent* InputComponent)
{
	check(InputComponent);
}


void ACharacter::GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (IsTemplate())
	{
		UE_LOG(LogCharacter, Log, TEXT("WARNING ACharacter::GetSimpleCollisionCylinder : Called on default object '%s'. Will likely return zero size. Consider using GetDefaultHalfHeight() instead."), *this->GetPathName());
	}
#endif

	if (RootComponent == CapsuleComponent && IsRootComponentCollisionRegistered())
	{
		// Note: we purposefully ignore the component transform here aside from scale, always treating it as vertically aligned.
		// This improves performance and is also how we stated the CapsuleComponent would be used.
		CapsuleComponent->GetScaledCapsuleSize(CollisionRadius, CollisionHalfHeight);
	}
	else
	{
		Super::GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
	}
}

void ACharacter::UpdateNavigationRelevance()
{
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
	}
}

void ACharacter::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	// Update cached location values in movement component
	if (CharacterMovement)
	{
		CharacterMovement->OldBaseLocation+= InOffset;
	}
}

float ACharacter::GetDefaultHalfHeight() const
{
	UCapsuleComponent* DefaultCapsule = GetClass()->GetDefaultObject<ACharacter>()->CapsuleComponent;
	if (DefaultCapsule)
	{
		return DefaultCapsule->GetScaledCapsuleHalfHeight();
	}
	else
	{
		return Super::GetDefaultHalfHeight();
	}
}


UActorComponent* ACharacter::FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
{
	// If the character has a Mesh, treat it as the first 'hit' when finding components
	if (Mesh && Mesh->IsA(ComponentClass))
	{
		return Mesh;
	}

	return Super::FindComponentByClass(ComponentClass);
}

void ACharacter::OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
{
}

void ACharacter::NotifyJumpApex()
{
	// Call delegate callback
	if (OnReachedJumpApex.IsBound())
	{
		OnReachedJumpApex.Broadcast();
	}
}

void ACharacter::Landed(const FHitResult& Hit)
{
	OnLanded(Hit);
}

bool ACharacter::CanJump() const
{
	return CanJumpInternal();
}

bool ACharacter::CanJumpInternal_Implementation() const
{
	const bool bCanHoldToJumpHigher = (GetJumpMaxHoldTime() > 0.0f) && IsJumpProvidingForce();

	return !bIsCrouched && CharacterMovement && (CharacterMovement->IsMovingOnGround() || bCanHoldToJumpHigher) && CharacterMovement->IsJumpAllowed() && !CharacterMovement->bWantsToCrouch;
}

void ACharacter::OnJumped_Implementation()
{
}

bool ACharacter::IsJumpProvidingForce() const
{
	return (bPressedJump && JumpKeyHoldTime > 0.0f && JumpKeyHoldTime < GetJumpMaxHoldTime());
}

// Deprecated
bool ACharacter::DoJump( bool bReplayingMoves )
{
	return CanJump() && CharacterMovement->DoJump(bReplayingMoves);
}


void ACharacter::RecalculateBaseEyeHeight()
{
	if (!bIsCrouched)
	{
		Super::RecalculateBaseEyeHeight();
	}
	else
	{
		BaseEyeHeight = CrouchedEyeHeight;
	}
}


void ACharacter::OnRep_IsCrouched()
{
	if (CharacterMovement)
	{
		if (bIsCrouched)
		{
			CharacterMovement->Crouch(true);
		}
		else
		{
			CharacterMovement->UnCrouch(true);
		}
	}
}

bool ACharacter::CanCrouch()
{
	return !bIsCrouched && CharacterMovement && CharacterMovement->CanEverCrouch() && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void ACharacter::Crouch(bool bClientSimulation)
{
	if (CharacterMovement)
	{
		if (CanCrouch())
		{
			CharacterMovement->bWantsToCrouch = true;
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		else if (!CharacterMovement->CanEverCrouch())
		{
			UE_LOG(LogCharacter, Log, TEXT("%s is trying to crouch, but crouching is disabled on this character! (check CharacterMovement NavAgentSettings)"), *GetName());
		}
#endif
	}
}

void ACharacter::UnCrouch(bool bClientSimulation)
{
	if (CharacterMovement)
	{
		CharacterMovement->bWantsToCrouch = false;
	}
}


void ACharacter::OnEndCrouch( float HeightAdjust, float ScaledHeightAdjust )
{
	RecalculateBaseEyeHeight();

	if (Mesh)
	{
		Mesh->RelativeLocation.Z = GetDefault<ACharacter>(GetClass())->Mesh->RelativeLocation.Z;
		BaseTranslationOffset.Z = Mesh->RelativeLocation.Z;
	}
	else
	{
		BaseTranslationOffset.Z = GetDefault<ACharacter>(GetClass())->BaseTranslationOffset.Z;
	}

	K2_OnEndCrouch(HeightAdjust, ScaledHeightAdjust);
}

void ACharacter::OnStartCrouch( float HeightAdjust, float ScaledHeightAdjust )
{
	RecalculateBaseEyeHeight();

	if (Mesh)
	{
		Mesh->RelativeLocation.Z = GetDefault<ACharacter>(GetClass())->Mesh->RelativeLocation.Z + HeightAdjust;
		BaseTranslationOffset.Z = Mesh->RelativeLocation.Z;
	}
	else
	{
		BaseTranslationOffset.Z = GetDefault<ACharacter>(GetClass())->BaseTranslationOffset.Z + HeightAdjust;
	}

	K2_OnStartCrouch(HeightAdjust, ScaledHeightAdjust);
}

void ACharacter::ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser)
{
	UDamageType const* const DmgTypeCDO = DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>();
	float const ImpulseScale = DmgTypeCDO->DamageImpulse;

	if ( (ImpulseScale > 3.f) && (CharacterMovement != NULL) )
	{
		FHitResult HitInfo;
		FVector ImpulseDir;
		DamageEvent.GetBestHitInfo(this, PawnInstigator, HitInfo, ImpulseDir);

		FVector Impulse = ImpulseDir * ImpulseScale;
		bool const bMassIndependentImpulse = !DmgTypeCDO->bScaleMomentumByMass;

		// limit Z momentum added if already going up faster than jump (to avoid blowing character way up into the sky)
		{
			FVector MassScaledImpulse = Impulse;
			if(!bMassIndependentImpulse && CharacterMovement->Mass > SMALL_NUMBER)
			{
				MassScaledImpulse = MassScaledImpulse / CharacterMovement->Mass;
			}

			if ( (CharacterMovement->Velocity.Z > GetDefault<UCharacterMovementComponent>(CharacterMovement->GetClass())->JumpZVelocity) && (MassScaledImpulse.Z > 0.f) )
			{
				Impulse.Z *= 0.5f;
			}
		}

		CharacterMovement->AddImpulse(Impulse, bMassIndependentImpulse);
	}
}

void ACharacter::ClearCrossLevelReferences()
{
	if( BasedMovement.MovementBase != NULL && GetOutermost() != BasedMovement.MovementBase->GetOutermost() )
	{
		SetBase( NULL );
	}

	Super::ClearCrossLevelReferences();
}

namespace MovementBaseUtility
{
	bool IsDynamicBase(const UPrimitiveComponent* MovementBase)
	{
		return (MovementBase && MovementBase->Mobility == EComponentMobility::Movable);
	}

	void AddTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* NewBase)
	{
		if (NewBase && MovementBaseUtility::UseRelativeLocation(NewBase))
		{
			if (NewBase->PrimaryComponentTick.bCanEverTick)
			{
				BasedObjectTick.AddPrerequisite(NewBase, NewBase->PrimaryComponentTick);
			}

			AActor* NewBaseOwner = NewBase->GetOwner();
			if (NewBaseOwner)
			{
				if (NewBaseOwner->PrimaryActorTick.bCanEverTick)
				{
					BasedObjectTick.AddPrerequisite(NewBaseOwner, NewBaseOwner->PrimaryActorTick);
				}

				// @TODO: We need to find a more efficient way of finding all ticking components in an actor.
				for (UActorComponent* Component : NewBaseOwner->GetComponents())
				{
					if (Component && Component->PrimaryComponentTick.bCanEverTick)
					{
						BasedObjectTick.AddPrerequisite(Component, Component->PrimaryComponentTick);
					}
				}
			}
		}
	}

	void RemoveTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* OldBase)
	{
		if (OldBase && MovementBaseUtility::UseRelativeLocation(OldBase))
		{
			BasedObjectTick.RemovePrerequisite(OldBase, OldBase->PrimaryComponentTick);
			AActor* OldBaseOwner = OldBase->GetOwner();
			if (OldBaseOwner)
			{
				BasedObjectTick.RemovePrerequisite(OldBaseOwner, OldBaseOwner->PrimaryActorTick);

				// @TODO: We need to find a more efficient way of finding all ticking components in an actor.
				for (UActorComponent* Component : OldBaseOwner->GetComponents())
				{
					if (Component && Component->PrimaryComponentTick.bCanEverTick)
					{
						BasedObjectTick.RemovePrerequisite(Component, Component->PrimaryComponentTick);
					}
				}
			}
		}
	}

	FVector GetMovementBaseVelocity(const UPrimitiveComponent* MovementBase, const FName BoneName)
	{
		FVector BaseVelocity = FVector::ZeroVector;
		if (MovementBaseUtility::IsDynamicBase(MovementBase))
		{
			if (BoneName != NAME_None)
			{
				const FBodyInstance* BodyInstance = MovementBase->GetBodyInstance(BoneName);
				if (BodyInstance)
				{
					BaseVelocity = BodyInstance->GetUnrealWorldVelocity();
					return BaseVelocity;
				}
			}

			BaseVelocity = MovementBase->GetComponentVelocity();
			if (BaseVelocity.IsZero())
			{
				// Fall back to actor's Root component
				const AActor* Owner = MovementBase->GetOwner();
				if (Owner)
				{
					// Component might be moved manually (not by simulated physics or a movement component), see if the root component of the actor has a velocity.
					BaseVelocity = MovementBase->GetOwner()->GetVelocity();
				}				
			}

			// Fall back to physics velocity.
			if (BaseVelocity.IsZero() && MovementBase->GetBodyInstance())
			{
				BaseVelocity = MovementBase->GetBodyInstance()->GetUnrealWorldVelocity();
			}
		}
		
		return BaseVelocity;
	}

	FVector GetMovementBaseTangentialVelocity(const UPrimitiveComponent* MovementBase, const FName BoneName, const FVector& WorldLocation)
	{
		if (MovementBaseUtility::IsDynamicBase(MovementBase))
		{
			if (const FBodyInstance* BodyInstance = MovementBase->GetBodyInstance(BoneName))
			{
				const FVector BaseAngVel = BodyInstance->GetUnrealWorldAngularVelocity();
				if (!BaseAngVel.IsNearlyZero())
				{
					FVector BaseLocation;
					FQuat BaseRotation;
					if (MovementBaseUtility::GetMovementBaseTransform(MovementBase, BoneName, BaseLocation, BaseRotation))
					{
						const FVector RadialDistanceToBase = WorldLocation - BaseLocation;
						const FVector TangentialVel = FVector::DegreesToRadians(BaseAngVel) ^ RadialDistanceToBase;
						return TangentialVel;
					}
				}
			}			
		}
		
		return FVector::ZeroVector;
	}

	bool GetMovementBaseTransform(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector& OutLocation, FQuat& OutQuat)
	{
		if (MovementBase)
		{
			if (BoneName != NAME_None)
			{
				const USkeletalMeshComponent* SkeletalBase = Cast<USkeletalMeshComponent>(MovementBase);
				if (SkeletalBase)
				{
					const int32 BoneIndex = SkeletalBase->GetBoneIndex(BoneName);
					if (BoneIndex != INDEX_NONE)
					{
						const FTransform BoneTransform = SkeletalBase->GetBoneTransform(BoneIndex);
						OutLocation = BoneTransform.GetLocation();
						OutQuat = BoneTransform.GetRotation();
						return true;
					}

					UE_LOG(LogCharacter, Warning, TEXT("GetMovementBaseTransform(): Invalid bone '%s' for SkeletalMeshComponent base %s"), *BoneName.ToString(), *GetPathNameSafe(MovementBase));
					return false;
				}
				// TODO: warn if not a skeletal mesh but providing bone index.
			}

			// No bone supplied
			OutLocation = MovementBase->GetComponentLocation();
			OutQuat = MovementBase->GetComponentQuat();
			return true;
		}

		// NULL MovementBase
		OutLocation = FVector::ZeroVector;
		OutQuat = FQuat::Identity;
		return false;
	}
}


/**	Change the Pawn's base. */
void ACharacter::SetBase( UPrimitiveComponent* NewBaseComponent, const FName InBoneName, bool bNotifyPawn )
{
	// If NewBaseComponent is null, ignore bone name.
	const FName BoneName = (NewBaseComponent ? InBoneName : NAME_None);

	// See what changed.
	const bool bBaseChanged = (NewBaseComponent != BasedMovement.MovementBase);
	const bool bBoneChanged = (BoneName != BasedMovement.BoneName);

	if (bBaseChanged || bBoneChanged)
	{
		// Verify no recursion.
		APawn* Loop = (NewBaseComponent ? Cast<APawn>(NewBaseComponent->GetOwner()) : NULL);
		for(  ; Loop!=NULL; Loop=Cast<APawn>(Loop->GetMovementBase()) )
		{
			if( Loop == this )
			{
				UE_LOG(LogCharacter, Warning, TEXT(" SetBase failed! Recursion detected. Pawn %s already based on %s."), *GetName(), *NewBaseComponent->GetName());
				return;
			}
		}

		// Set base.
		UPrimitiveComponent* OldBase = BasedMovement.MovementBase;
		BasedMovement.MovementBase = NewBaseComponent;
		BasedMovement.BoneName = BoneName;

		if (CharacterMovement)
		{
			if (bBaseChanged)
			{
				MovementBaseUtility::RemoveTickDependency(CharacterMovement->PrimaryComponentTick, OldBase);
				MovementBaseUtility::AddTickDependency(CharacterMovement->PrimaryComponentTick, NewBaseComponent);
			}

			if (NewBaseComponent)
			{
				// Update OldBaseLocation/Rotation as those were referring to a different base
				// ... but not when handling replication for proxies (since they are going to copy this data from the replicated values anyway)
				if (!bInBaseReplication)
				{					
					CharacterMovement->SaveBaseLocation();
				}

				// Enable pre-cloth tick if we are standing on a physics object, as we need to to use post-physics transforms
				CharacterMovement->PreClothComponentTick.SetTickFunctionEnable(NewBaseComponent->IsSimulatingPhysics());
			}
			else
			{
				BasedMovement.BoneName = NAME_None; // None, regardless of whether user tried to set a bone name, since we have no base component.
				BasedMovement.bRelativeRotation = false;
				CharacterMovement->CurrentFloor.Clear();
				CharacterMovement->PreClothComponentTick.SetTickFunctionEnable(false);
			}

			if (Role == ROLE_Authority || Role == ROLE_AutonomousProxy)
			{
				BasedMovement.bServerHasBaseComponent = (BasedMovement.MovementBase != NULL); // Also set on proxies for nicer debugging.
				UE_LOG(LogCharacter, Verbose, TEXT("Setting base on %s for '%s' to '%s'"), Role == ROLE_Authority ? TEXT("Server") : TEXT("AutoProxy"), *GetName(), *GetFullNameSafe(NewBaseComponent));
			}
			else
			{
				UE_LOG(LogCharacter, Verbose, TEXT("Setting base on Client for '%s' to '%s'"), *GetName(), *GetFullNameSafe(NewBaseComponent));
			}

		}

		// Notify this actor of his new floor.
		if ( bNotifyPawn )
		{
			BaseChange();
		}
	}
}


void ACharacter::SaveRelativeBasedMovement(const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation)
{
	checkSlow(BasedMovement.HasRelativeLocation());
	BasedMovement.Location = NewRelativeLocation;
	BasedMovement.Rotation = NewRotation;
	BasedMovement.bRelativeRotation = bRelativeRotation;
}

FVector ACharacter::GetNavAgentLocation() const
{
	FVector AgentLocation = FNavigationSystem::InvalidLocation;

	if (GetCharacterMovement() != nullptr)
	{
		AgentLocation = GetCharacterMovement()->GetActorFeetLocation();
	}

	if (FNavigationSystem::IsValidLocation(AgentLocation) == false && CapsuleComponent != nullptr)
	{
		AgentLocation = GetActorLocation() - FVector(0, 0, CapsuleComponent->GetScaledCapsuleHalfHeight());
	}

	return AgentLocation;
}

void ACharacter::TurnOff()
{
	if (CharacterMovement != NULL)
	{
		CharacterMovement->StopMovementImmediately();
		CharacterMovement->DisableMovement();
	}

	if (GetNetMode() != NM_DedicatedServer && Mesh != NULL)
	{
		Mesh->bPauseAnims = true;
		if (Mesh->IsSimulatingPhysics())
		{
			Mesh->bBlendPhysics = true;
			Mesh->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipAllBones;
		}
	}

	Super::TurnOff();
}

void ACharacter::Restart()
{
	Super::Restart();

	bPressedJump = false;
	JumpKeyHoldTime = 0.0f;
	ClearJumpInput();
	UnCrouch(true);

	if (CharacterMovement)
	{
		CharacterMovement->SetDefaultMovementMode();
	}
}

void ACharacter::PawnClientRestart()
{
	if (CharacterMovement != NULL)
	{
		CharacterMovement->StopMovementImmediately();
		CharacterMovement->ResetPredictionData_Client();
	}

	Super::PawnClientRestart();
}

void ACharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// If we are controlled remotely, set animation timing to be driven by client's network updates. So timing and events remain in sync.
	if (Mesh && (GetRemoteRole() == ROLE_AutonomousProxy && GetNetConnection() != nullptr))
	{
		Mesh->bAutonomousTickPose = true;
	}
}

void ACharacter::UnPossessed()
{
	Super::UnPossessed();

	if (CharacterMovement)
	{
		CharacterMovement->ResetPredictionData_Client();
		CharacterMovement->ResetPredictionData_Server();
	}

	// We're no longer controlled remotely, resume regular ticking of animations.
	if (Mesh)
	{
		Mesh->bAutonomousTickPose = false;
	}
}


void ACharacter::TornOff()
{
	Super::TornOff();

	if (CharacterMovement)
	{
		CharacterMovement->ResetPredictionData_Client();
		CharacterMovement->ResetPredictionData_Server();
	}

	// We're no longer controlled remotely, resume regular ticking of animations.
	if (Mesh)
	{
		Mesh->bAutonomousTickPose = false;
	}
}


void ACharacter::BaseChange()
{
	if (CharacterMovement && CharacterMovement->MovementMode != MOVE_None)
	{
		AActor* ActualMovementBase = GetMovementBaseActor(this);
		if ((ActualMovementBase != NULL) && !ActualMovementBase->CanBeBaseForCharacter(this))
		{
			CharacterMovement->JumpOff(ActualMovementBase);
		}
	}
}

void ACharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	float Indent = 0.f;

	UFont* RenderFont = GEngine->GetSmallFont();

	static FName NAME_Physics = FName(TEXT("Physics"));
	if (DebugDisplay.IsDisplayOn(NAME_Physics) )
	{
		FIndenter PhysicsIndent(Indent);

		FString BaseString;
		if ( CharacterMovement == NULL || BasedMovement.MovementBase == NULL )
		{
			BaseString = "Not Based";
		}
		else
		{
			BaseString = BasedMovement.MovementBase->IsWorldGeometry() ? "World Geometry" : BasedMovement.MovementBase->GetName();
			BaseString = FString::Printf(TEXT("Based On %s"), *BaseString);
		}
		
		YL = Canvas->DrawText(RenderFont, FString::Printf(TEXT("RelativeLoc: %s Rot: %s %s"), *BasedMovement.Location.ToString(), *BasedMovement.Rotation.ToString(), *BaseString), Indent, YPos);
		YPos += YL;

		if ( CharacterMovement != NULL )
		{
			CharacterMovement->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
		const bool Crouched = CharacterMovement && CharacterMovement->IsCrouching();
		FString T = FString::Printf(TEXT("Crouched %i"), Crouched);
		YL = Canvas->DrawText(RenderFont, T, Indent, YPos );
		YPos += YL;
	}
}

void ACharacter::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	UE_LOG(LogCharacter, Verbose, TEXT("ACharacter::LaunchCharacter '%s' (%f,%f,%f)"), *GetName(), LaunchVelocity.X, LaunchVelocity.Y, LaunchVelocity.Z);

	if (CharacterMovement)
	{
		FVector FinalVel = LaunchVelocity;
		const FVector Velocity = GetVelocity();

		if (!bXYOverride)
		{
			FinalVel.X += Velocity.X;
			FinalVel.Y += Velocity.Y;
		}
		if (!bZOverride)
		{
			FinalVel.Z += Velocity.Z;
		}

		CharacterMovement->Launch(FinalVel);

		OnLaunched(LaunchVelocity, bXYOverride, bZOverride);
	}
}


void ACharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PrevCustomMode)
{
	K2_OnMovementModeChanged(PrevMovementMode, CharacterMovement->MovementMode, PrevCustomMode, CharacterMovement->CustomMovementMode);
	MovementModeChangedDelegate.Broadcast(this, PrevMovementMode, PrevCustomMode);
}


/** Don't process landed notification if updating client position by replaying moves. 
 * Allow event to be called if Pawn was initially falling (before starting to replay moves), 
 * and this is going to cause him to land. . */
bool ACharacter::ShouldNotifyLanded(const FHitResult& Hit)
{
	if (bClientUpdating && !bClientWasFalling)
	{
		return false;
	}

	// Just in case, only allow Landed() to be called once when replaying moves.
	bClientWasFalling = false;
	return true;
}

void ACharacter::Jump()
{
	bPressedJump = true;
	JumpKeyHoldTime = 0.0f;
}

void ACharacter::StopJumping()
{
	bPressedJump = false;
	JumpKeyHoldTime = 0.0f;
}

void ACharacter::CheckJumpInput(float DeltaTime)
{
	const bool bWasJumping = bPressedJump && JumpKeyHoldTime > 0.0f;
	if (bPressedJump)
	{
		// Increment our timer first so calls to IsJumpProvidingForce() will return true
		JumpKeyHoldTime += DeltaTime;
		const bool bDidJump = CanJump() && CharacterMovement && CharacterMovement->DoJump(bClientUpdating);
		if (!bWasJumping && bDidJump)
		{
			OnJumped();
		}
	}
}


void ACharacter::ClearJumpInput()
{
	// Don't disable bPressedJump right away if it's still held
	if (bPressedJump && (JumpKeyHoldTime >= GetJumpMaxHoldTime()))
	{
		bPressedJump = false;
	}
}

float ACharacter::GetJumpMaxHoldTime() const
{
	return JumpMaxHoldTime;
}

//
// Static variables for networking.
//
static uint8 SavedMovementMode;

void ACharacter::PreNetReceive()
{
	SavedMovementMode = ReplicatedMovementMode;
	Super::PreNetReceive();
}

void ACharacter::PostNetReceive()
{
	if (Role == ROLE_SimulatedProxy)
	{
		CharacterMovement->bNetworkUpdateReceived = true;
		CharacterMovement->bNetworkMovementModeChanged = (CharacterMovement->bNetworkMovementModeChanged || (SavedMovementMode != ReplicatedMovementMode));
	}

	Super::PostNetReceive();
}

void ACharacter::OnRep_ReplicatedBasedMovement()
{	
	if (Role != ROLE_SimulatedProxy)
	{
		return;
	}

	// Skip base updates while playing root motion, it is handled inside of OnRep_RootMotion
	if (IsPlayingNetworkedRootMotionMontage())
	{
		return;
	}

	TGuardValue<bool> bInBaseReplicationGuard(bInBaseReplication, true);

	const bool bBaseChanged = (BasedMovement.MovementBase != ReplicatedBasedMovement.MovementBase || BasedMovement.BoneName != ReplicatedBasedMovement.BoneName);
	if (bBaseChanged)
	{
		// Even though we will copy the replicated based movement info, we need to use SetBase() to set up tick dependencies and trigger notifications.
		SetBase(ReplicatedBasedMovement.MovementBase, ReplicatedBasedMovement.BoneName);
	}

	// Make sure to use the values of relative location/rotation etc from the server.
	BasedMovement = ReplicatedBasedMovement;

	if (ReplicatedBasedMovement.HasRelativeLocation())
	{
		// Update transform relative to movement base
		const FVector OldLocation = GetActorLocation();
		MovementBaseUtility::GetMovementBaseTransform(ReplicatedBasedMovement.MovementBase, ReplicatedBasedMovement.BoneName, CharacterMovement->OldBaseLocation, CharacterMovement->OldBaseQuat);
		const FVector NewLocation = CharacterMovement->OldBaseLocation + ReplicatedBasedMovement.Location;

		if (ReplicatedBasedMovement.HasRelativeRotation())
		{
			// Relative location, relative rotation
			FRotator NewRotation = (FRotationMatrix(ReplicatedBasedMovement.Rotation) * FQuatRotationMatrix(CharacterMovement->OldBaseQuat)).Rotator();
			
			// TODO: need a better way to not assume we only use Yaw.
			NewRotation.Pitch = 0.f;
			NewRotation.Roll = 0.f;

			SetActorLocationAndRotation(NewLocation, NewRotation);	
		}
		else
		{
			// Relative location, absolute rotation
			SetActorLocationAndRotation(NewLocation, ReplicatedBasedMovement.Rotation);
		}

		// When position or base changes, movement mode will need to be updated. This assumes rotation changes don't affect that.
		CharacterMovement->bJustTeleported |= (bBaseChanged || GetActorLocation() != OldLocation);

		INetworkPredictionInterface* PredictionInterface = Cast<INetworkPredictionInterface>(GetMovementComponent());
		if (PredictionInterface)
		{
			PredictionInterface->SmoothCorrection(OldLocation);
		}
	}
}

void ACharacter::OnRep_ReplicatedMovement()
{
	// Skip standard position correction if we are playing root motion, OnRep_RootMotion will handle it.
	if (!IsPlayingNetworkedRootMotionMontage())
	{
		Super::OnRep_ReplicatedMovement();
	}
}

/** Get FAnimMontageInstance playing RootMotion */
FAnimMontageInstance * ACharacter::GetRootMotionAnimMontageInstance() const
{
	return (Mesh && Mesh->AnimScriptInstance) ? Mesh->AnimScriptInstance->GetRootMotionMontageInstance() : NULL;
}

void ACharacter::OnRep_RootMotion()
{
	if( Role == ROLE_SimulatedProxy )
	{
		UE_LOG(LogRootMotion, Log,  TEXT("ACharacter::OnRep_RootMotion"));

		// Save received move in queue, we'll try to use it during Tick().
		if( RepRootMotion.AnimMontage )
		{
			// Add new move
			FSimulatedRootMotionReplicatedMove NewMove;
			NewMove.RootMotion = RepRootMotion;
			NewMove.Time = GetWorld()->GetTimeSeconds();
			RootMotionRepMoves.Add(NewMove);
		}
		else
		{
			// Clear saved moves.
			RootMotionRepMoves.Empty();
		}
	}
}

void ACharacter::SimulatedRootMotionPositionFixup(float DeltaSeconds)
{
	const FAnimMontageInstance* ClientMontageInstance = GetRootMotionAnimMontageInstance();
	if( ClientMontageInstance && CharacterMovement && Mesh )
	{
		// Find most recent buffered move that we can use.
		const int32 MoveIndex = FindRootMotionRepMove(*ClientMontageInstance);
		if( MoveIndex != INDEX_NONE )
		{
			const FVector OldLocation = GetActorLocation();
			// Move Actor back to position of that buffered move. (server replicated position).
			const FSimulatedRootMotionReplicatedMove& RootMotionRepMove = RootMotionRepMoves[MoveIndex];
			if( RestoreReplicatedMove(RootMotionRepMove) )
			{
				const float ServerPosition = RootMotionRepMove.RootMotion.Position;
				const float ClientPosition = ClientMontageInstance->GetPosition();
				const float DeltaPosition = (ClientPosition - ServerPosition);
				if( FMath::Abs(DeltaPosition) > KINDA_SMALL_NUMBER )
				{
					// Find Root Motion delta move to get back to where we were on the client.
					const FTransform LocalRootMotionTransform = ClientMontageInstance->Montage->ExtractRootMotionFromTrackRange(ServerPosition, ClientPosition);

					// Simulate Root Motion for delta move.
					if( CharacterMovement )
					{
						const float MontagePlayRate = ClientMontageInstance->GetPlayRate();
						// Guess time it takes for this delta track position, so we can get falling physics accurate.
						if (!FMath::IsNearlyZero(MontagePlayRate))
						{
							const float DeltaTime = DeltaPosition / MontagePlayRate;

							// Even with negative playrate deltatime should be positive.
							check(DeltaTime > 0.f);
							CharacterMovement->SimulateRootMotion(DeltaTime, LocalRootMotionTransform);

							// After movement correction, smooth out error in position if any.
							INetworkPredictionInterface* PredictionInterface = Cast<INetworkPredictionInterface>(GetMovementComponent());
							if (PredictionInterface)
							{
								PredictionInterface->SmoothCorrection(OldLocation);
							}
						}
					}
				}
			}

			// Delete this move and any prior one, we don't need them anymore.
			UE_LOG(LogRootMotion, Log,  TEXT("\tClearing old moves (%d)"), MoveIndex+1);
			RootMotionRepMoves.RemoveAt(0, MoveIndex+1);
		}
	}
}

int32 ACharacter::FindRootMotionRepMove(const FAnimMontageInstance& ClientMontageInstance) const
{
	int32 FoundIndex = INDEX_NONE;

	// Start with most recent move and go back in time to find a usable move.
	for(int32 MoveIndex=RootMotionRepMoves.Num()-1; MoveIndex>=0; MoveIndex--)
	{
		if( CanUseRootMotionRepMove(RootMotionRepMoves[MoveIndex], ClientMontageInstance) )
		{
			FoundIndex = MoveIndex;
			break;
		}
	}

	UE_LOG(LogRootMotion, Log,  TEXT("\tACharacter::FindRootMotionRepMove FoundIndex: %d, NumSavedMoves: %d"), FoundIndex, RootMotionRepMoves.Num());
	return FoundIndex;
}

bool ACharacter::CanUseRootMotionRepMove(const FSimulatedRootMotionReplicatedMove& RootMotionRepMove, const FAnimMontageInstance& ClientMontageInstance) const
{
	// Ignore outdated moves.
	if( GetWorld()->TimeSince(RootMotionRepMove.Time) <= 0.5f )
	{
		// Make sure montage being played matched between client and server.
		if( RootMotionRepMove.RootMotion.AnimMontage && (RootMotionRepMove.RootMotion.AnimMontage == ClientMontageInstance.Montage) )
		{
			UAnimMontage * AnimMontage = ClientMontageInstance.Montage;
			const float ServerPosition = RootMotionRepMove.RootMotion.Position;
			const float ClientPosition = ClientMontageInstance.GetPosition();
			const float DeltaPosition = (ClientPosition - ServerPosition);
			const int32 CurrentSectionIndex = AnimMontage->GetSectionIndexFromPosition(ClientPosition);
			if( CurrentSectionIndex != INDEX_NONE )
			{
				const int32 NextSectionIndex = (CurrentSectionIndex < ClientMontageInstance.NextSections.Num()) ? ClientMontageInstance.NextSections[CurrentSectionIndex] : INDEX_NONE;

				// We can only extract root motion if we are within the same section.
				// It's not trivial to jump through sections in a deterministic manner, but that is luckily not frequent. 
				const bool bSameSections = (AnimMontage->GetSectionIndexFromPosition(ServerPosition) == CurrentSectionIndex);
				// if we are looping and just wrapped over, skip. That's also not easy to handle and not frequent.
				const bool bHasLooped = (NextSectionIndex == CurrentSectionIndex) && (FMath::Abs(DeltaPosition) > (AnimMontage->GetSectionLength(CurrentSectionIndex) / 2.f));
				// Can only simulate forward in time, so we need to make sure server move is not ahead of the client.
				const bool bServerAheadOfClient = ((DeltaPosition * ClientMontageInstance.GetPlayRate()) < 0.f);

				UE_LOG(LogRootMotion, Log,  TEXT("\t\tACharacter::CanUseRootMotionRepMove ServerPosition: %.3f, ClientPosition: %.3f, DeltaPosition: %.3f, bSameSections: %d, bHasLooped: %d, bServerAheadOfClient: %d"), 
					ServerPosition, ClientPosition, DeltaPosition, bSameSections, bHasLooped, bServerAheadOfClient);

				return bSameSections && !bHasLooped && !bServerAheadOfClient;
			}
		}
	}
	return false;
}

bool ACharacter::RestoreReplicatedMove(const FSimulatedRootMotionReplicatedMove& RootMotionRepMove)
{
	UPrimitiveComponent* ServerBase = RootMotionRepMove.RootMotion.MovementBase;
	const FName ServerBaseBoneName = RootMotionRepMove.RootMotion.MovementBaseBoneName;

	// Relative Position
	if( RootMotionRepMove.RootMotion.bRelativePosition )
	{
		bool bSuccess = false;
		if( MovementBaseUtility::UseRelativeLocation(ServerBase) )
		{
			FVector BaseLocation;
			FQuat BaseRotation;
			MovementBaseUtility::GetMovementBaseTransform(ServerBase, ServerBaseBoneName, BaseLocation, BaseRotation);

			const FVector ServerLocation = BaseLocation + RootMotionRepMove.RootMotion.Location;
			FRotator ServerRotation;
			if (RootMotionRepMove.RootMotion.bRelativeRotation)
			{
				// Relative rotation
				ServerRotation = (FRotationMatrix(RootMotionRepMove.RootMotion.Rotation) * FQuatRotationTranslationMatrix(BaseRotation, FVector::ZeroVector)).Rotator();
			}
			else
			{
				// Absolute rotation
				ServerRotation = RootMotionRepMove.RootMotion.Rotation;
			}

			UpdateSimulatedPosition(ServerLocation, ServerRotation);
			bSuccess = true;
		}
		// If we received local space position, but can't resolve parent, then move can't be used. :(
		if( !bSuccess )
		{
			return false;
		}
	}
	// Absolute position
	else
	{
		UpdateSimulatedPosition(RootMotionRepMove.RootMotion.Location, RootMotionRepMove.RootMotion.Rotation);
	}

	SetBase( ServerBase, ServerBaseBoneName );

	return true;
}

void ACharacter::UpdateSimulatedPosition(const FVector& NewLocation, const FRotator& NewRotation)
{
	// Always consider Location as changed if we were spawned this tick as in that case our replicated Location was set as part of spawning, before PreNetReceive()
	if( (NewLocation != GetActorLocation()) || (CreationTime == GetWorld()->TimeSeconds) )
	{
		FVector FinalLocation = NewLocation;
		if( GetWorld()->EncroachingBlockingGeometry(this, NewLocation, NewRotation) )
		{
			bSimGravityDisabled = true;
		}
		else
		{
			bSimGravityDisabled = false;
		}
		
		// Don't use TeleportTo(), that clears our base.
		SetActorLocationAndRotation(FinalLocation, NewRotation, false);
		CharacterMovement->bJustTeleported = true;
	}
	else if( NewRotation != GetActorRotation() )
	{
		GetRootComponent()->MoveComponent(FVector::ZeroVector, NewRotation, false);
	}
}

void ACharacter::PostNetReceiveLocationAndRotation()
{
	if( Role == ROLE_SimulatedProxy )
	{
		// Don't change transform if using relative position (it should be nearly the same anyway, or base may be slightly out of sync)
		if (!ReplicatedBasedMovement.HasRelativeLocation())
		{
			const FVector OldLocation = GetActorLocation();
			UpdateSimulatedPosition(ReplicatedMovement.Location, ReplicatedMovement.Rotation);

			INetworkPredictionInterface* PredictionInterface = Cast<INetworkPredictionInterface>(GetMovementComponent());
			if (PredictionInterface)
			{
				PredictionInterface->SmoothCorrection(OldLocation);
			}
		}
	}
}

void ACharacter::PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker )
{
	Super::PreReplication( ChangedPropertyTracker );

	const FAnimMontageInstance* RootMotionMontageInstance = GetRootMotionAnimMontageInstance();

	if ( RootMotionMontageInstance )
	{
		// Is position stored in local space?
		RepRootMotion.bRelativePosition = BasedMovement.HasRelativeLocation();
		RepRootMotion.bRelativeRotation = BasedMovement.HasRelativeRotation();
		RepRootMotion.Location			= RepRootMotion.bRelativePosition ? BasedMovement.Location : GetActorLocation();
		RepRootMotion.Rotation			= RepRootMotion.bRelativeRotation ? BasedMovement.Rotation : GetActorRotation();
		RepRootMotion.MovementBase		= BasedMovement.MovementBase;
		RepRootMotion.MovementBaseBoneName = BasedMovement.BoneName;
		RepRootMotion.AnimMontage		= RootMotionMontageInstance->Montage;
		RepRootMotion.Position			= RootMotionMontageInstance->GetPosition();

		DOREPLIFETIME_ACTIVE_OVERRIDE( ACharacter, RepRootMotion, true );
	}
	else
	{
		RepRootMotion.Clear();

		DOREPLIFETIME_ACTIVE_OVERRIDE( ACharacter, RepRootMotion, false );
	}

	ReplicatedMovementMode = CharacterMovement->PackNetworkMovementMode();	
	ReplicatedBasedMovement = BasedMovement;

	// Optimization: only update and replicate these values if they are actually going to be used.
	if (BasedMovement.HasRelativeLocation())
	{
		// When velocity becomes zero, force replication so the position is updated to match the server (it may have moved due to simulation on the client).
		ReplicatedBasedMovement.bServerHasVelocity = !CharacterMovement->Velocity.IsZero();

		// Make sure absolute rotations are updated in case rotation occurred after the base info was saved.
		if (!BasedMovement.HasRelativeRotation())
		{
			ReplicatedBasedMovement.Rotation = GetActorRotation();
		}
	}
}

void ACharacter::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME_CONDITION( ACharacter, RepRootMotion,				COND_SimulatedOnly );
	DOREPLIFETIME_CONDITION( ACharacter, ReplicatedBasedMovement,	COND_SimulatedOnly );
	DOREPLIFETIME_CONDITION( ACharacter, ReplicatedMovementMode,	COND_SimulatedOnly );
	DOREPLIFETIME_CONDITION( ACharacter, bIsCrouched,				COND_SimulatedOnly );
}

bool ACharacter::IsPlayingRootMotion() const
{
	if (Mesh && Mesh->AnimScriptInstance)
	{
		return	(Mesh->AnimScriptInstance->RootMotionMode == ERootMotionMode::RootMotionFromEverything) ||
				(Mesh->AnimScriptInstance->GetRootMotionMontageInstance() != NULL);
	}
	return false;
}

bool ACharacter::IsPlayingNetworkedRootMotionMontage() const
{
	if (Mesh && Mesh->AnimScriptInstance)
	{
		return	(Mesh->AnimScriptInstance->RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly) &&
			(Mesh->AnimScriptInstance->GetRootMotionMontageInstance() != NULL);
	}
	return false;
}

float ACharacter::PlayAnimMontage(class UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : NULL; 
	if( AnimMontage && AnimInstance )
	{
		float const Duration = AnimInstance->Montage_Play(AnimMontage, InPlayRate);

		if (Duration > 0.f)
		{
			// Start at a given Section.
			if( StartSectionName != NAME_None )
			{
				AnimInstance->Montage_JumpToSection(StartSectionName);
			}

			return Duration;
		}
	}	

	return 0.f;
}

void ACharacter::StopAnimMontage(class UAnimMontage* AnimMontage)
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : NULL; 
	UAnimMontage * MontageToStop = (AnimMontage)? AnimMontage : GetCurrentMontage();
	bool bShouldStopMontage =  AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if ( bShouldStopMontage )
	{
		AnimInstance->Montage_Stop(MontageToStop->BlendOutTime, MontageToStop);
	}
}

class UAnimMontage * ACharacter::GetCurrentMontage()
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : NULL; 
	if ( AnimInstance )
	{
		return AnimInstance->GetCurrentActiveMontage();
	}

	return NULL;
}

void ACharacter::ClientCheatWalk_Implementation()
{
	SetActorEnableCollision(true);
	if (CharacterMovement)
	{
		CharacterMovement->bCheatFlying = false;
		CharacterMovement->SetMovementMode(MOVE_Falling);
	}
}

void ACharacter::ClientCheatFly_Implementation()
{
	SetActorEnableCollision(true);
	if (CharacterMovement)
	{
		CharacterMovement->bCheatFlying = true;
		CharacterMovement->SetMovementMode(MOVE_Flying);
	}
}

void ACharacter::ClientCheatGhost_Implementation()
{
	SetActorEnableCollision(false);
	if (CharacterMovement)
	{
		CharacterMovement->bCheatFlying = true;
		CharacterMovement->SetMovementMode(MOVE_Flying);
	}
}

/** Returns Mesh subobject **/
USkeletalMeshComponent* ACharacter::GetMesh() const { return Mesh; }
#if WITH_EDITORONLY_DATA
/** Returns ArrowComponent subobject **/
UArrowComponent* ACharacter::GetArrowComponent() const { return ArrowComponent; }
#endif
/** Returns CharacterMovement subobject **/
UCharacterMovementComponent* ACharacter::GetCharacterMovement() const { return CharacterMovement; }
/** Returns CapsuleComponent subobject **/
UCapsuleComponent* ACharacter::GetCapsuleComponent() const { return CapsuleComponent; }
