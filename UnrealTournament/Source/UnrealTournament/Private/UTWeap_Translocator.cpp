// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Translocator.h"
#include "UTProj_TransDisk.h"
#include "UTWeaponStateFiringOnce.h"
#include "UnrealNetwork.h"
#include "UTReachSpec_HighJump.h"
#include "UTWeaponRedirector.h"
#include "UTTranslocatorMessage.h"
#include "UTHUDWidget_Powerups.h"
#include "UTReplicatedEmitter.h"
#include "StatNames.h"

AUTWeap_Translocator::AUTWeap_Translocator(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTWeaponStateFiringOnce>(TEXT("FiringState0")).SetDefaultSubobjectClass<UUTWeaponStateFiringOnce>(TEXT("FiringState1")))
{
	TelefragDamage = 1337.0f;
	BringUpTime = 0.28f;
	PutDownTime = 0.2f;
	AmmoCost.Add(0);
	AmmoCost.Add(1);
	RecallFireInterval = 0.1f;
	BaseAISelectRating = -1.0f; // AI shouldn't select this unless wanted by pathing
	AfterImageType = AUTWeaponRedirector::StaticClass();
	TranslocatorMessageClass = UUTTranslocatorMessage::StaticClass();
	FOVOffset = FVector(1.2f, 1.f, 3.f);
	ShotPitchUp = 0.f;
	DiskGravity = 1.f;
	DefaultGroup = 0;
	KillStatsName = NAME_TelefragKills;
	DeathStatsName = NAME_TelefragDeaths;
	DisplayName = NSLOCTEXT("UTWeap_Translocator", "DisplayName", "Telefrag");

	FirstFireInterval = 0.12f;
	MinFastTranslocInterval = 0.7f;
}

void AUTWeap_Translocator::PostInitProperties()
{
	Super::PostInitProperties();
	Group = DefaultGroup;
}

void AUTWeap_Translocator::UpdateHUDText()
{
	FString NewTranslocatorKeyString("");

	if (UTOwner)
	{
		AUTPlayerController* PlayerController = Cast<AUTPlayerController>(UTOwner->GetController());
		if (PlayerController)
		{
			UUTPlayerInput* UTPlayerInput = Cast<UUTPlayerInput>(PlayerController->PlayerInput);
			if (UTPlayerInput)
			{
				bool foundTranslocatorAlready = false;

				TArray<FCustomKeyBinding>& CustomBinds = UTPlayerInput->CustomBinds;
				for (int index = 0; index < CustomBinds.Num(); ++index)
				{
					if (CustomBinds[index].KeyName.ToString() != "None")
					{
						if (CustomBinds[index].Command == "ToggleTranslocator")
						{
							NewTranslocatorKeyString = CustomBinds[index].KeyName.ToString();

							//If we have found a keybind for ToggleTranslocator don't bother going through the rest
							break;
						}
						//Only use SelectTranslocator if we haven't already found a valid ToggleTranslocator above
						else if ((CustomBinds[index].Command == "SelectTranslocator") && (NewTranslocatorKeyString == ""))
						{
							NewTranslocatorKeyString = CustomBinds[index].KeyName.ToString();
						}
					}
				}
			}
		}
	}

	HUDText = FText::FromString(NewTranslocatorKeyString);
}

bool AUTWeap_Translocator::HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget)
{
	return (Cast<UUTHUDWidget_Powerups>(TargetWidget) != NULL);
}

void AUTWeap_Translocator::ConsumeAmmo(uint8 FireModeNum)
{
}

void AUTWeap_Translocator::OnRep_TransDisk()
{
	if (FakeTransDisk)
	{
		FakeTransDisk->Destroy();
		FakeTransDisk = NULL;
	}
}

void AUTWeap_Translocator::ClearDisk()
{
	if (TransDisk != NULL)
	{
		TransDisk->Explode(TransDisk->GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
	TransDisk = NULL;
	
	if (GetUTOwner() != NULL)
	{
		// reset bHasTranslocator if it was false due to disrupted disk
		AUTBot* B = Cast<AUTBot>(GetUTOwner()->Controller);
		if (B != NULL)
		{
			B->bHasTranslocator = true;
		}
	}
}

void AUTWeap_Translocator::RecallDisk()
{
	if (Role == ROLE_Authority)
	{
		//remove disk server-side
		ClearDisk();

		// server side since can be picked up by server movement as well
		UUTGameplayStatics::UTPlaySound(GetWorld(), RecallSound, UTOwner, SRT_All);
	}
	if (FakeTransDisk)
	{
		FakeTransDisk->Destroy();
		FakeTransDisk = NULL;
	}

	// special recovery time for recall
	UUTWeaponStateFiringOnce* CurrentFiringState = Cast<UUTWeaponStateFiringOnce>(CurrentState);
	if (CurrentFiringState && GetWorldTimerManager().IsTimerActive(CurrentFiringState->RefireCheckHandle))
	{
		typedef void(UUTWeaponState::*WeaponTimerFunc)(void);
		GetWorldTimerManager().SetTimer(CurrentFiringState->RefireCheckHandle, CurrentFiringState, (WeaponTimerFunc)&UUTWeaponStateFiring::RefireCheckTimer, RecallFireInterval, false);
	}
}

FRotator AUTWeap_Translocator::GetAdjustedAim_Implementation(FVector StartFireLoc)
{
	FRotator BaseAim = Super::GetAdjustedAim_Implementation(StartFireLoc);
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL)
	{
		BaseAim.Pitch += (90.f - FMath::Min(1.f, FMath::Abs(BaseAim.Pitch))) * ShotPitchUp/90.f;
	}
	return BaseAim;
}

void AUTWeap_Translocator::FireShot()
{
	UTOwner->DeactivateSpawnProtection();
	if (!FireShotOverride() && GetUTOwner() != NULL) // script event may kill user
	{
		if (CurrentFireMode == 0)
		{
			//No disk. Shoot one
			if (TransDisk == NULL || TransDisk->IsPendingKillPending())
			{
				if (FakeTransDisk == NULL || FakeTransDisk->IsPendingKillPending())
				{
					ConsumeAmmo(CurrentFireMode);
					if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
					{
						TransDisk = Cast<AUTProj_TransDisk>(FireProjectile());
						if (Role != ROLE_Authority)
						{
							// wait for disk to be replicated from server
							AUTPlayerController* OwningPlayer = UTOwner ? Cast<AUTPlayerController>(UTOwner->GetController()) : NULL;
							if (OwningPlayer && OwningPlayer->UTPlayerState)
							{
								FakeTransDisk = TransDisk;
								float FakeDiskLife = (OwningPlayer && FakeTransDisk) ? 0.0015f * (OwningPlayer->UTPlayerState->ExactPing + OwningPlayer->PredictionFudgeFactor) : 0.001f* (OwningPlayer->MaxPredictionPing + OwningPlayer->PredictionFudgeFactor);
								if (OwningPlayer && FakeTransDisk)
								{
									FakeTransDisk->SetLifeSpan(FMath::Min(TransDisk->GetLifeSpan(), FMath::Max(0.f, FakeDiskLife)));
								}
							}
							TransDisk = NULL;
						}

						if (TransDisk != NULL)
						{
							TransDisk->MyTranslocator = this;
						}
					}
					UUTGameplayStatics::UTPlaySound(GetWorld(), ThrowSound, UTOwner, SRT_AllButOwner);

					// special recovery time for first shot
					if (GetWorld()->GetTimeSeconds() - LastTranslocTime > MinFastTranslocInterval)
					{
						UUTWeaponStateFiringOnce* CurrentFiringState = Cast<UUTWeaponStateFiringOnce>(CurrentState);
						if (CurrentFiringState && GetWorldTimerManager().IsTimerActive(CurrentFiringState->RefireCheckHandle))
						{
							typedef void(UUTWeaponState::*WeaponTimerFunc)(void);
							GetWorldTimerManager().SetTimer(CurrentFiringState->RefireCheckHandle, CurrentFiringState, (WeaponTimerFunc)&UUTWeaponStateFiring::RefireCheckTimer, FirstFireInterval, false);
						}
					}
				}
				else
				{
					FakeTransDisk->Destroy();
					FakeTransDisk = NULL;
				}
			}
			else if (TransDisk->TransState == TLS_Disrupted)
			{
				// can't recall disrupted disk
				UUTGameplayStatics::UTPlaySound(GetWorld(), DisruptedSound, UTOwner, SRT_AllButOwner);
				if (Cast<AUTPlayerController>(UTOwner->GetController()) && UTOwner->IsLocallyControlled())
				{
					Cast<AUTPlayerController>(UTOwner->GetController())->SendPersonalMessage(TranslocatorMessageClass, 0, NULL, NULL, NULL);
				}
				// end bot's move here if it requires translocation since it's not going to be able to refire to try again
				AUTBot* B = Cast<AUTBot>(UTOwner->GetController());
				if (B != NULL && Cast<UUTReachSpec_HighJump>(B->GetCurrentPath().Spec.Get()) != NULL)
				{
					B->MoveTimer = -1.0f;
				}
			}
			else
			{
				RecallDisk();
			}
			if (TransDisk && TransDisk->ProjectileMovement)
			{
				TransDisk->ProjectileMovement->ProjectileGravityScale = DiskGravity;
			}
		}
		else if (TransDisk != NULL)
		{
			if (TransDisk->TransState == TLS_Disrupted)
			{
				ConsumeAmmo(CurrentFireMode); // well, we're probably about to die, but just in case

				FUTPointDamageEvent Event;
				float AdjustedMomentum = 1000.0f;
				Event.Damage = TelefragDamage;
				Event.DamageTypeClass = TransFailDamageType;
				Event.HitInfo = FHitResult(UTOwner, UTOwner->GetCapsuleComponent(), UTOwner->GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
				Event.ShotDirection = GetVelocity().GetSafeNormal();
				Event.Momentum = Event.ShotDirection * AdjustedMomentum;

				UTOwner->TakeDamage(TelefragDamage, Event, TransDisk->DisruptedController, UTOwner);
			}
			else
			{
				UTOwner->IncrementFlashCount(CurrentFireMode);

				if ((Role == ROLE_Authority) || (UTOwner && Cast<AUTPlayerController>(UTOwner->GetController()) && (Cast<AUTPlayerController>(UTOwner->GetController())->GetPredictionTime() > 0.f)))
				{
					UPrimitiveComponent* SavedPlayerBase = UTOwner->GetMovementBase();
					FTransform SavedPlayerTransform = UTOwner->GetTransform();
					FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(UTOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), UTOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
					FVector WarpLocation = TransDisk->GetActorLocation();
					FHitResult Hit;
					FVector EndTrace = WarpLocation - FVector(0.0f, 0.0f, PlayerCapsule.GetCapsuleHalfHeight());
					float SweepRadius = TransDisk->CollisionComp->GetCollisionShape().GetSphereRadius();
					bool bHitFloor = GetWorld()->SweepSingleByChannel(Hit, WarpLocation, EndTrace, FQuat::Identity, UTOwner->GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(SweepRadius), FCollisionQueryParams(FName(TEXT("Translocation")), false, UTOwner), UTOwner->GetCapsuleComponent()->GetCollisionResponseToChannels());
					if (bHitFloor)
					{
						// need to more teleport destination up, unless close to ceiling
						FVector NewLocation = Hit.Location + FVector(0.0f, 0.0f, PlayerCapsule.GetCapsuleHalfHeight());
						bool bHitCeiling = GetWorld()->SweepSingleByChannel(Hit, WarpLocation, NewLocation, FQuat::Identity, UTOwner->GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(SweepRadius), FCollisionQueryParams(FName(TEXT("Translocation")), false, UTOwner), UTOwner->GetCapsuleComponent()->GetCollisionResponseToChannels());
						if (!bHitCeiling)
						{
							WarpLocation = NewLocation;
						}
					}
					FRotator WarpRotation(0.0f, UTOwner->GetActorRotation().Yaw, 0.0f);

					ECollisionChannel SavedObjectType = UTOwner->GetCapsuleComponent()->GetCollisionObjectType();
					UTOwner->GetCapsuleComponent()->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);
					//UE_LOG(UT, Warning, TEXT("Translocate to %f %f %f"), WarpLocation.X, WarpLocation.Y, WarpLocation.Z);
					// test first so we don't drop the flag on an unsuccessful teleport
					if (GetWorld()->FindTeleportSpot(UTOwner, WarpLocation, WarpRotation))
					{
						UTOwner->GetCapsuleComponent()->SetCollisionObjectType(SavedObjectType);
						if (Role == ROLE_Authority)
						{
							AUTCarriedObject* Flag = UTOwner->GetCarriedObject();
							UTOwner->DropFlag();
							if (Flag)
							{
								Flag->MovementComponent->Velocity = FVector(0.f ,0.f, FMath::Min(Flag->MovementComponent->Velocity.Z, 0.f));
							}
						}
						UTOwner->bIsTranslocating = true;  // different telefrag rules than for teleporters

						// You can die during teleportation, UTOwner is not guaranteed valid
						AUTCharacter* SavedOwner = UTOwner;
						if (UTOwner->TeleportTo(WarpLocation, WarpRotation))
						{
							LastTranslocTime = GetWorld()->GetTimeSeconds();
							ConsumeAmmo(CurrentFireMode);
							if (UTOwner && UTOwner->UTCharacterMovement && UTOwner->UTCharacterMovement->bIsFloorSliding)
							{
								float VelZ = UTOwner->UTCharacterMovement->Velocity.Z;
								UTOwner->UTCharacterMovement->Velocity *= 0.9f;
								UTOwner->UTCharacterMovement->Velocity.Z = VelZ;
							}
							// spawn effects
							FActorSpawnParameters SpawnParams;
							SpawnParams.Instigator = SavedOwner;
							SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
							SpawnParams.Owner = SavedOwner;
							if (AfterImageType != NULL)
							{
								AUTWeaponRedirector* AfterImage = GetWorld()->SpawnActor<AUTWeaponRedirector>(AfterImageType, SavedPlayerTransform.GetLocation(), SavedPlayerTransform.GetRotation().Rotator(), SpawnParams);
								if (AfterImage != NULL)
								{
									AfterImage->InitFor(SavedOwner, FRepCollisionShape(PlayerCapsule), SavedPlayerBase, SavedOwner->GetTransform());
								}
							}
							if (DestinationEffect != NULL)
							{
								GetWorld()->SpawnActor<AUTReplicatedEmitter>(DestinationEffect, SavedOwner->GetActorLocation(), SavedOwner->GetActorRotation(), SpawnParams);
							}
						}
						SavedOwner->bIsTranslocating = false;
					}
					else
					{
						UTOwner->GetCapsuleComponent()->SetCollisionObjectType(SavedObjectType);
					}
				}
				UUTGameplayStatics::UTPlaySound(GetWorld(), TeleSound, UTOwner, SRT_AllButOwner);
			}
			ClearDisk();
			if (FakeTransDisk != nullptr)
			{
				FakeTransDisk->Destroy();
				FakeTransDisk = nullptr;
			}
		}
		else if (FakeTransDisk != nullptr)
		{
			FakeTransDisk->Destroy();
			FakeTransDisk = nullptr;
		}

		PlayFiringEffects();
	}

	if (GetUTOwner() != NULL)
	{
		GetUTOwner()->InventoryEvent(InventoryEventName::FiredWeapon);
	}
	FireZOffsetTime = 0.f; 
}

bool AUTWeap_Translocator::ShouldDropOnDeath()
{
	return false;
}

void AUTWeap_Translocator::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	Destroy();
}

void AUTWeap_Translocator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	ClearDisk();
}

void AUTWeap_Translocator::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeap_Translocator, TransDisk, COND_None);
}

void AUTWeap_Translocator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// check for AI shooting disc at translocator target
	if (CurrentState == ActiveState && (TransDisk == NULL || TransDisk->IsPendingKillPending()))
	{
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if (B != NULL && !B->TranslocTarget.IsZero() && (Cast<UUTReachSpec_HighJump>(B->GetCurrentPath().Spec.Get()) == NULL || B->GetMovePoint() != B->GetMoveTarget().GetLocation(UTOwner)) && !B->NeedToTurn(B->GetFocalPoint(), true))
		{
			// fire disk
			UTOwner->StartFire(0);
			if (UTOwner != nullptr)
			{
				UTOwner->StopFire(0);
			}
		}
	}
}

void AUTWeap_Translocator::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	AUTBot* B = Cast<AUTBot>(NewOwner->Controller);
	if (B != NULL)
	{
		B->bHasTranslocator = true;
		if (ProjClass.IsValidIndex(0) && ProjClass[0] != NULL)
		{
			B->TransDiscTemplate = ProjClass[0].GetDefaultObject();
		}
	}
}

float AUTWeap_Translocator::GetAISelectRating_Implementation()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL || !B->AllowTranslocator())
	{
		return BaseAISelectRating;
	}
	else if (!B->TranslocTarget.IsZero())
	{
		return 9.1f;
	}
	else
	{
		UUTReachSpec_HighJump* JumpSpec = Cast<UUTReachSpec_HighJump>(B->GetCurrentPath().Spec.Get());
		if (JumpSpec != NULL && JumpSpec->CalcAvailableSimpleJumpZ(UTOwner) < JumpSpec->CalcRequiredJumpZ(UTOwner) && (B->GetEnemy() == NULL || B->GetMovePoint() == B->GetMoveTarget().GetLocation(UTOwner) || !B->LineOfSightTo(B->GetEnemy())))
		{
			return 9.1f;
		}
		else if (UTOwner->GetWeapon() == this && B->TranslocInterval < 1.0f && !B->IsStopped() && (B->GetTarget() == NULL || B->GetFocusActor() != B->GetTarget()))
		{
			// leave translocator out for now so bot can spam teleports if desired
			return 1.0f;
		}
		else
		{
			return BaseAISelectRating;
		}
	}
}

bool AUTWeap_Translocator::DoAssistedJump()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL)
	{
		return false;
	}
	else
	{
		// TODO: merge this code with the translocation-specific checks in AUTBot::ApplyWeaponAimAdjust(), add AUTBot::PickTranslocatorTossForPath() or similiar
		if (B->TranslocTarget.IsZero())
		{
			const FVector BaseMoveTarget = B->GetMoveTarget().GetLocation(UTOwner);
			// get list of potential navmesh polys in this path to toss to, find best one we can actually hit
			if (ProjClass.IsValidIndex(0) && ProjClass[0] != NULL && ProjClass[0].GetDefaultObject()->ProjectileMovement != NULL && ProjClass[0].GetDefaultObject()->CollisionComp != NULL)
			{
				const AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
				TArray<FVector> PotentialTargets;
				PotentialTargets.Add(BaseMoveTarget);
				for (NavNodeRef Poly : B->GetCurrentPath().AdditionalEndPolys)
				{
					if (Poly != B->GetMoveTarget().TargetPoly)
					{
						PotentialTargets.Add(NavData->GetPolyCenter(Poly) + FVector(0.0f, 0.0f, UTOwner->GetSimpleCollisionHalfHeight()));
					}
				}
				bool bAddedFallbackTarget = false;
				{
					// find poly wall closest to shooter location and use that as a fallback throw target
					FVector HitLoc;
					if (NavData->Raycast(B->GetMoveTarget().GetLocation(NULL), UTOwner->GetActorLocation(), HitLoc, NavData->GetDefaultQueryFilter()))
					{
						float ZDiff = BaseMoveTarget.Z - HitLoc.Z;
						HitLoc.Z += ZDiff * 0.5f;
						FVector Extent = UTOwner->GetSimpleCollisionCylinderExtent() * FVector(2.0f, 2.0f, 1.0f);
						Extent.Z += FMath::Abs<float>(ZDiff) * 0.5f;
						NavNodeRef WallPoly = NavData->UTFindNearestPoly(HitLoc, Extent);
						if (WallPoly != INVALID_NAVNODEREF)
						{
							TArray<FLine> Walls = NavData->GetPolyWalls(WallPoly);
							if (Walls.Num() > 0)
							{
								FVector TestLoc = BaseMoveTarget;
								float BestDist = FLT_MAX;
								for (const FLine& TestWall : Walls)
								{
									float Dist = (TestWall.GetCenter() - UTOwner->GetActorLocation()).Size();
									if (Dist < BestDist)
									{
										TestLoc = TestWall.GetCenter();
										BestDist = Dist;
									}
								}
								TestLoc.Z += UTOwner->GetSimpleCollisionHalfHeight();
								PotentialTargets.Add(TestLoc);
								bAddedFallbackTarget = true;
							}
						}
					}
				}

				AUTProjectile* DefaultProj = ProjClass[0].GetDefaultObject();
				const float ProjRadius = DefaultProj->CollisionComp->GetUnscaledSphereRadius();
				const float GravityZ = UTOwner->GetCharacterMovement()->GetGravityZ() * DefaultProj->ProjectileMovement->ProjectileGravityScale;

				bool bFound = false;
				for (const FVector& TestLoc : PotentialTargets)
				{
					FVector StartLoc = UTOwner->GetActorLocation() + (TestLoc - UTOwner->GetActorLocation()).Rotation().RotateVector(FireOffset);
					// if firing upward, add minimum possible TossZ contribution to effective speed to improve toss prediction
					float EffectiveSpeed = DefaultProj->ProjectileMovement->InitialSpeed;
					if (DefaultProj->TossZ > 0.0f)
					{
						EffectiveSpeed += FMath::Max<float>(0.0f, (TestLoc - StartLoc).GetSafeNormal().Z * DefaultProj->TossZ);
					}
					FVector TossVel;
					if (UUTGameplayStatics::UTSuggestProjectileVelocity(this, TossVel, StartLoc, TestLoc, NULL, FLT_MAX, EffectiveSpeed, ProjRadius, GravityZ))
					{
						// TODO: assemble successful toss list, allow bot to choose best to its goal?
						B->TranslocTarget = TestLoc;
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					const FVector TestLoc = bAddedFallbackTarget ? PotentialTargets.Last() : BaseMoveTarget;
					// see if there's a better throw by strafing to the side
					const FVector Side = 500.0f * ((TestLoc - UTOwner->GetActorLocation()).GetSafeNormal() ^ FVector(0.0f, 0.0f, 1.0f));
					const FVector TestPoints[] = { UTOwner->GetNavAgentLocation() + Side, UTOwner->GetNavAgentLocation() - Side, UTOwner->GetNavAgentLocation() + (UTOwner->GetNavAgentLocation() - TestLoc).GetSafeNormal2D() * 500.0f };
					for (FVector NewStart : TestPoints)
					{
						NavNodeRef EndPoly = INVALID_NAVNODEREF;
						FVector HitLoc;
						if (NavData->RaycastWithZCheck(UTOwner->GetNavAgentLocation(), NewStart, &HitLoc, &EndPoly))
						{
							NewStart = HitLoc;
						}
						if (EndPoly != INVALID_NAVNODEREF)
						{
							NewStart.Z = NavData->GetPolyZAtLoc(EndPoly, FVector2D(NewStart)) + UTOwner->GetSimpleCollisionHalfHeight();
							float EffectiveSpeed = DefaultProj->ProjectileMovement->InitialSpeed;
							if (DefaultProj->TossZ > 0.0f)
							{
								EffectiveSpeed += FMath::Max<float>(0.0f, (TestLoc - NewStart).GetSafeNormal().Z * DefaultProj->TossZ);
							}
							FVector TossVel;
							if (UUTGameplayStatics::UTSuggestProjectileVelocity(this, TossVel, NewStart, TestLoc, NULL, FLT_MAX, EffectiveSpeed, ProjRadius, GravityZ))
							{
								B->TranslocTarget = TestLoc;
								B->SetAdjustLoc(NewStart);
								bFound = true;
								break;
							}
						}
					}
					if (!bFound)
					{
						// try default anyway
						// TODO: mark as probable failure, tag ReachSpec if bot in fact doesn't make it
						B->TranslocTarget = BaseMoveTarget;
					}
				}
			}
			else
			{
				B->TranslocTarget = BaseMoveTarget;
			}
		}
		// look at target
		FVector LookPoint = B->TranslocTarget;
		// account for projectile toss
		B->NextFireMode = 0; // make sure bot uses the right mode for arc detection
		B->ApplyWeaponAimAdjust(LookPoint, LookPoint);
		if (B->NeedToTurn(LookPoint, true))
		{
			B->SetFocalPoint(B->TranslocTarget, SCRIPTEDMOVE_FOCUS_PRIORITY);
			return false; // not ready yet
		}
		else
		{
			if (TransDisk == NULL || TransDisk->IsPendingKillPending())
			{
				// shoot!
				UTOwner->StartFire(0);
				UTOwner->StopFire(0);
			}
			return false;
		}
	}
}

bool AUTWeap_Translocator::CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc)
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL || B->GetFocusActor() == Target)
	{
		bool bResult = Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, BestFireMode, OptimalTargetLoc);
		BestFireMode = 0;
		return bResult;
	}
	else
	{
		// when using translocator for movement other code will handle firing, don't use normal path
		BestFireMode = 0;
		return false;
	}
}