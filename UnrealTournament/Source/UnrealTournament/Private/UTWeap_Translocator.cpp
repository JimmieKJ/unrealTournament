// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Translocator.h"
#include "UTProj_TransDisk.h"
#include "UTWeaponStateFiringOnce.h"
#include "UnrealNetwork.h"
#include "UTReachSpec_HighJump.h"

AUTWeap_Translocator::AUTWeap_Translocator(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateFiringOnce>(TEXT("FiringState0")).SetDefaultSubobjectClass<UUTWeaponStateFiringOnce>(TEXT("FiringState1")))
{
	if (FiringState.Num() > 1)
	{
#if WITH_EDITORONLY_DATA
		FiringStateType[0] = UUTWeaponStateFiringOnce::StaticClass();
		FiringStateType[1] = UUTWeaponStateFiringOnce::StaticClass();
#endif
	}
	TelefragDamage = 1337.0f;

	BringUpTime = 0.32f;
	PutDownTime = 0.2f;

	AmmoCost.Add(0);
	AmmoCost.Add(1);
	Ammo = 8;
	MaxAmmo = 8;
	AmmoRechargeRate = 1.0f;

	RecallFireInterval = 0.1f;

	BaseAISelectRating = -1.0f; // AI shouldn't select this unless wanted by pathing
}

void AUTWeap_Translocator::DrawWeaponInfo_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	if (Ammo < 1)
	{
		UFont* Font = WeaponHudWidget->UTHUDOwner->MediumFont;
		FString AmmoStr = FString::Printf(TEXT("OVERHEAT"));
		FText AmmoText = FText::FromString(AmmoStr);
		WeaponHudWidget->DrawText(AmmoText, 0, 0, Font, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Bottom);
	}
}

void AUTWeap_Translocator::ConsumeAmmo(uint8 FireModeNum)
{
	Super::ConsumeAmmo(FireModeNum);

	if ((FireModeNum == 1 || Ammo < MaxAmmo) && !GetWorldTimerManager().IsTimerActive(this, &AUTWeap_Translocator::RechargeTimer) && !bPendingKillPending)
	{
		GetWorldTimerManager().SetTimer(this, &AUTWeap_Translocator::RechargeTimer, AmmoRechargeRate, true);
	}
}

void AUTWeap_Translocator::RechargeTimer()
{
	AddAmmo(1);
	if (Ammo >= MaxAmmo)
	{
		GetWorldTimerManager().ClearTimer(this, &AUTWeap_Translocator::RechargeTimer);
	}
}
void AUTWeap_Translocator::OnRep_Ammo()
{
	Super::OnRep_Ammo();
	if (Ammo >= MaxAmmo)
	{
		GetWorldTimerManager().ClearTimer(this, &AUTWeap_Translocator::RechargeTimer);
	}
}

void AUTWeap_Translocator::OnRep_TransDisk()
{

}

void AUTWeap_Translocator::ClearDisk()
{
	if (TransDisk != NULL)
	{
		TransDisk->Explode(TransDisk->GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
	TransDisk = NULL;
}

void AUTWeap_Translocator::FireShot()
{
	UTOwner->DeactivateSpawnProtection();
	if (!FireShotOverride() && GetUTOwner() != NULL) // script event may kill user
	{
		if (CurrentFireMode == 0)
		{
			//No disk. Shoot one
			if (TransDisk == NULL)
			{
				ConsumeAmmo(CurrentFireMode);
				if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
				{
					TransDisk = Cast<AUTProj_TransDisk>(FireProjectile());

					if (TransDisk != NULL)
					{
						TransDisk->MyTranslocator = this;
					}
				}

				UUTGameplayStatics::UTPlaySound(GetWorld(), ThrowSound, UTOwner, SRT_AllButOwner);
			}
			else
			{
				//remove disk
				ClearDisk();

				UUTGameplayStatics::UTPlaySound(GetWorld(), RecallSound, UTOwner, SRT_AllButOwner);

				// special recovery time for recall
				typedef void(UUTWeaponState::*WeaponTimerFunc)(void);
				if (Cast<UUTWeaponStateFiringOnce>(CurrentState) != NULL && GetWorldTimerManager().IsTimerActive(CurrentState, (WeaponTimerFunc)&UUTWeaponStateFiring::RefireCheckTimer))
				{
					GetWorldTimerManager().SetTimer(CurrentState, (WeaponTimerFunc)&UUTWeaponStateFiring::RefireCheckTimer, RecallFireInterval, false);
				}
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
				Event.HitInfo = FHitResult(UTOwner, UTOwner->CapsuleComponent, UTOwner->GetActorLocation(), FVector(0.0f,0.0f,1.0f));
				Event.ShotDirection = GetVelocity().SafeNormal();
				Event.Momentum = Event.ShotDirection * AdjustedMomentum;

				UTOwner->TakeDamage(TelefragDamage, Event, TransDisk->DisruptedController, UTOwner);
			}
			else
			{
				UTOwner->IncrementFlashCount(CurrentFireMode);

				if ((Role == ROLE_Authority) || (UTOwner && Cast<AUTPlayerController>(UTOwner->GetController()) && (Cast<AUTPlayerController>(UTOwner->GetController())->GetPredictionTime() > 0.f)))
				{
					FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(UTOwner->CapsuleComponent->GetUnscaledCapsuleRadius(), UTOwner->CapsuleComponent->GetUnscaledCapsuleHalfHeight());
					FVector WarpLocation = TransDisk->GetActorLocation();
					FHitResult Hit;
					FVector EndTrace = WarpLocation + FVector(0.0f, 0.0f, 2.f* PlayerCapsule.GetCapsuleHalfHeight());
					bool bHitGeometry = GetWorld()->SweepSingle(Hit, WarpLocation, EndTrace, FQuat::Identity, UTOwner->CapsuleComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(TransDisk->CollisionComp->GetCollisionShape().GetSphereRadius()), FCollisionQueryParams(FName(TEXT("Translocation")), false, UTOwner), UTOwner->CapsuleComponent->GetCollisionResponseToChannels());
					WarpLocation = 0.5f * (WarpLocation + (bHitGeometry ? Hit.Location : EndTrace));
					FRotator WarpRotation(0.0f, UTOwner->GetActorRotation().Yaw, 0.0f);

					ECollisionChannel SavedObjectType = UTOwner->CapsuleComponent->GetCollisionObjectType();
					UTOwner->CapsuleComponent->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);
					//UE_LOG(UT, Warning, TEXT("Translocate to %f %f %f"), WarpLocation.X, WarpLocation.Y, WarpLocation.Z);
					// test first so we don't drop the flag on an unsuccessful teleport
					if (GetWorld()->FindTeleportSpot(UTOwner, WarpLocation, WarpRotation))
					{
						UTOwner->CapsuleComponent->SetCollisionObjectType(SavedObjectType);
						UTOwner->DropFlag();

						if (UTOwner->TeleportTo(WarpLocation, WarpRotation))
						{
							ConsumeAmmo(CurrentFireMode);
						}
					}
					else
					{
						UTOwner->CapsuleComponent->SetCollisionObjectType(SavedObjectType);
					}
				}
				UUTGameplayStatics::UTPlaySound(GetWorld(), TeleSound, UTOwner, SRT_AllButOwner);
			}
			ClearDisk();
		}

		PlayFiringEffects();
	}
	else
	{
		ConsumeAmmo(CurrentFireMode);
	}
	if (GetUTOwner() != NULL)
	{
		static FName NAME_FiredWeapon(TEXT("FiredWeapon"));
		GetUTOwner()->InventoryEvent(NAME_FiredWeapon);
	}
}

//Dont drop Weapon when killed
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
	if (CurrentState == ActiveState && (TransDisk == NULL || TransDisk->bPendingKillPending))
	{
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if (B != NULL && !B->TranslocTarget.IsZero() && (Cast<UUTReachSpec_HighJump>(B->GetCurrentPath().Spec.Get()) == NULL || B->GetMovePoint() != B->GetMoveTarget().GetLocation(UTOwner)) && !B->NeedToTurn(B->GetFocalPoint(), true))
		{
			// fire disk
			UTOwner->StartFire(0);
			UTOwner->StopFire(0);
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
		else if (UTOwner->GetWeapon() == this && B->TranslocInterval < 1.0f && !B->IsStopped() && (B->GetEnemy() == NULL || !B->LineOfSightTo(B->GetEnemy())))
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
						NavNodeRef WallPoly = NavData->FindNearestPoly(HitLoc, Extent);
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
				const float GravityZ = UTOwner->CharacterMovement->GetGravityZ() * DefaultProj->ProjectileMovement->ProjectileGravityScale;
				TArray<AActor*> IgnoreActors;
				IgnoreActors.Add(UTOwner);

				bool bFound = false;
				for (const FVector& TestLoc : PotentialTargets)
				{
					FVector StartLoc = UTOwner->GetActorLocation() + (TestLoc - UTOwner->GetActorLocation()).Rotation().RotateVector(FireOffset);
					// if firing upward, add minimum possible TossZ contribution to effective speed to improve toss prediction
					float EffectiveSpeed = DefaultProj->ProjectileMovement->InitialSpeed;
					if (DefaultProj->TossZ > 0.0f)
					{
						EffectiveSpeed += FMath::Max<float>(0.0f, (TestLoc - StartLoc).SafeNormal().Z * DefaultProj->TossZ);
					}
					FVector TossVel;
					if (UGameplayStatics::SuggestProjectileVelocity(this, TossVel, StartLoc, TestLoc, EffectiveSpeed, false, ProjRadius, GravityZ, ESuggestProjVelocityTraceOption::TraceFullPath, DefaultProj->CollisionComp->GetCollisionResponseToChannels(), IgnoreActors))
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
					const FVector Side = 500.0f * ((TestLoc - UTOwner->GetActorLocation()).SafeNormal() ^ FVector(0.0f, 0.0f, 1.0f));
					const FVector TestPoints[] = { UTOwner->GetNavAgentLocation() + Side, UTOwner->GetNavAgentLocation() - Side, UTOwner->GetNavAgentLocation() + (UTOwner->GetNavAgentLocation() - TestLoc).SafeNormal2D() * 500.0f };
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
								EffectiveSpeed += FMath::Max<float>(0.0f, (TestLoc - NewStart).SafeNormal().Z * DefaultProj->TossZ);
							}
							FVector TossVel;
							if (UGameplayStatics::SuggestProjectileVelocity(this, TossVel, NewStart, TestLoc, EffectiveSpeed, false, ProjRadius, GravityZ, ESuggestProjVelocityTraceOption::TraceFullPath, DefaultProj->CollisionComp->GetCollisionResponseToChannels(), IgnoreActors))
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
		B->ApplyWeaponAimAdjust(LookPoint, LookPoint);
		if (B->NeedToTurn(LookPoint, true))
		{
			B->SetFocalPoint(B->TranslocTarget, true, SCRIPTEDMOVE_FOCUS_PRIORITY);
			return false; // not ready yet
		}
		else
		{
			if (TransDisk == NULL || TransDisk->bPendingKillPending)
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
		return false;
	}
}