// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTLift.h"
#include "UTCharacterMovement.h"
#include "UTGib.h"
#include "UTDroppedPickupImportant.h"
#include "NavigationOctree.h"
#include "UTLiftExit.h"
#include "UTDmgType_Crushed.h"
#include "UTReachSpec_Lift.h"
#include "UTRecastNavMesh.h"

AUTLift::AUTLift(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	LastEncroachNotifyTime = -10.f;
	bAlwaysRelevant = true;
	LiftVelocity = FVector(0.f);
	TickLocation = EncroachComponent ? EncroachComponent->GetComponentLocation() : GetActorLocation();
	NavmeshScale = 0.75f;
	NetPriority = 2.7f;
	bReplicates = true;
	bCanBeDamaged = false;
}

void AUTLift::SetEncroachComponent(class UPrimitiveComponent* NewEncroachComponent)
{
	if (EncroachComponent && EncroachComponent->OnComponentBeginOverlap.IsBound())
	{
		EncroachComponent->OnComponentBeginOverlap.RemoveDynamic(this, &AUTLift::OnOverlapBegin);
	} 
	EncroachComponent = NewEncroachComponent; 
	if (EncroachComponent)
	{
		EncroachComponent->bGenerateOverlapEvents = true;
		EncroachComponent->OnComponentBeginOverlap.AddDynamic(this, &AUTLift::OnOverlapBegin);
	}
}

void AUTLift::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		//UE_LOG(UT, Warning, TEXT("Overlapping %s relative position %f"), *OtherActor->GetName(), GetActorLocation().Z - OtherActor->GetActorLocation().Z);
		if (Cast<APawn>(OtherActor))
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(OtherActor);
			if (UTChar && UTChar->IsRagdoll())
			{
				FUTPointDamageEvent DamageEvent(100000.0f, FHitResult(UTChar, UTChar->GetCapsuleComponent(), UTChar->GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), UUTDmgType_Crushed::StaticClass());
				UTChar->TakeDamage(100000.0f, DamageEvent, UTChar->GetController(), UTChar);
			}
			else
			{
				OnEncroachActor(OtherActor);
			}
		}
	}
}

void AUTLift::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bSelfMoved && EncroachComponent)
	{
		//if (GetWorld()->GetTimeSeconds() - LastEncroachNotifyTime > 0.2f)
		{
			//if (Other) { UE_LOG(UT, Warning, TEXT("RECEIVE HIT %s"), *Other->GetName()); }
			AUTProjectile* Proj = Cast<AUTProjectile>(Other);
			if (Proj != NULL)
			{
				if (bMoveWasBlocked)
				{
					// projectile didn't get out of the way
					Proj->ImpactedActor = this;
					Proj->Explode(Proj->GetActorLocation(), -HitNormal, MyComp);
					Proj->ImpactedActor = NULL;
				}
				else if (Cast<UUTProjectileMovementComponent>(Proj->ProjectileMovement) != nullptr && Proj->ProjectileMovement->bShouldBounce)
				{
					// try to move projectile by amount lift moved so that next attempt won't be blocked and trigger bounce event to change velocity
					if (Proj->SetActorLocation(Proj->GetActorLocation() + (Hit.TraceEnd - Hit.TraceStart), false) && !Proj->bExploded)
					{
						((UUTProjectileMovementComponent*)Proj->ProjectileMovement)->SimulateImpact(FHitResult(Proj, Proj->ProjectileMovement->UpdatedPrimitive, Hit.Location, -Hit.Normal));
					}
				}
				bMoveWasBlocked = true;
				return;
			}
			AUTCharacter* UTChar = Cast<AUTCharacter>(Other);
			if (UTChar && UTChar->IsDead())
			{
				UTChar->Destroy();
				return;
			}
			if (UTChar && UTChar->IsRagdoll())
			{
				if (bMoveWasBlocked)
				{
					FUTPointDamageEvent DamageEvent(100000.0f, FHitResult(UTChar, UTChar->GetCapsuleComponent(), UTChar->GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), UUTDmgType_Crushed::StaticClass());
					UTChar->TakeDamage(100000.0f, DamageEvent, UTChar->GetController(), UTChar);
				}
				bMoveWasBlocked = true;
				return;
			}
			if (UTChar && UTChar->UTCharacterMovement && UTChar->UTCharacterMovement->CanBaseOnLift(EncroachComponent, LiftEndLocation - EncroachComponent->GetComponentLocation()))
			{
				// if UTCharacter could stand on me, then base him and keep going
				bMoveWasBlocked = true;
				return;
			}
			// if bot controlled character, tell it to move out from under the lift
			if (UTChar != NULL)
			{
				AUTBot* B = Cast<AUTBot>(UTChar->Controller);
				if (B != NULL && Cast<UUTReachSpec_Lift>(B->GetCurrentPath().Spec.Get()) != NULL)
				{
					AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
					NavNodeRef Poly = NavData->FindAnchorPoly(UTChar->GetNavAgentLocation(), UTChar, UTChar->GetNavAgentPropertiesRef());
					if (Poly != INVALID_NAVNODEREF)
					{
						TArray<NavNodeRef> AdjacentPolys;
						NavData->FindAdjacentPolys(UTChar, UTChar->GetNavAgentPropertiesRef(), Poly, true, AdjacentPolys);

						FCollisionShape TestShape = UTChar->GetCapsuleComponent()->GetCollisionShape();
						TestShape.SetCapsule(TestShape.GetCapsuleRadius(), 1000.0f);

						for (NavNodeRef DestPoly : AdjacentPolys)
						{
							FVector TestLoc = NavData->GetPolySurfaceCenter(DestPoly);
							if (!EncroachComponent->OverlapComponent(TestLoc, FQuat::Identity, TestShape))
							{
								B->GoalString = TEXT("Move out from under lift");
								// we don't pass the poly here so the AI will always hit the exact location and not consider it reached at the poly edge which may not be far enough away
								B->SetMoveTargetDirect(FRouteCacheItem(TestLoc + FVector(0.0f, 0.0f, UTChar->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())));
								break;
							}
						}
					}
				}
			}
			AUTDroppedPickup* Pickup = Cast<AUTDroppedPickup>(Other);
			if (Pickup != NULL && Pickup->Movement != NULL && Pickup->Movement->UpdatedComponent != NULL && !bMoveWasBlocked && (LiftVelocity.Z > 0.f || Other->GetActorLocation().Z > MyComp->GetCenterOfMass().Z))
			{
				Pickup->Movement->StopSimulating(Hit);
				Pickup->AttachToComponent(EncroachComponent, FAttachmentTransformRules::KeepWorldTransform, NAME_None);
				bMoveWasBlocked = true;
				return;
			}
			AUTDroppedPickupImportant* ImportantPickup = Cast<AUTDroppedPickupImportant>(Pickup);
			if (ImportantPickup != NULL)
			{
				ImportantPickup->MoveToSafeLocation(EncroachComponent->Bounds.GetBox().ExpandBy(100.0f));
				bMoveWasBlocked = true;
				return;
			}
			if (Cast<AUTGib>(Other) != NULL || Pickup != NULL || Cast<AUTCosmetic>(Other) != NULL)
			{
				if (!Other->Destroy())
				{
					// on client, destroy will fail, but we still need to prevent the collision to avoid a desync of the lift position
					UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Other->GetRootComponent());
					if (RootPrim != NULL)
					{
						RootPrim->SetCollisionResponseToChannel(EncroachComponent->GetCollisionObjectType(), ECR_Ignore);
					}
				}
				bMoveWasBlocked = true;
				return;
			}
			if (Cast<AUTCarriedObject>(Other))
			{
				AUTCarriedObject* DroppedFlag = Cast<AUTCarriedObject>(Other);
				if ((LiftVelocity.Z <= 0.f) && (DroppedFlag->GetActorLocation().Z < MyComp->GetCenterOfMass().Z))
				{
					DroppedFlag->SendHomeWithNotify();
				}
				else
				{
					DroppedFlag->MovementComponent->StopSimulating(Hit);
					DroppedFlag->AttachToComponent(EncroachComponent, FAttachmentTransformRules::KeepWorldTransform, NAME_None);
				}
				if (!bMoveWasBlocked)
				{
					bMoveWasBlocked = true;
					return;
				}
			}

			OnEncroachActor(Other);
			LastEncroachNotifyTime = GetWorld()->GetTimeSeconds();
		}
	}
}

// @TODO FIXMESTEVE support lifts on other lifts, relative movement, and rotation
void AUTLift::MoveLiftTo(FVector NewLocation, FRotator NewRotation)
{
	if (EncroachComponent && (!NewLocation.Equals(EncroachComponent->GetComponentLocation()) || !NewRotation.Equals(EncroachComponent->GetComponentRotation())))
	{
		bMoveWasBlocked = false;
		LiftStartLocation = EncroachComponent->GetComponentLocation();
		LiftEndLocation = NewLocation;
		EncroachComponent->MoveComponent(NewLocation - EncroachComponent->GetComponentLocation(), EncroachComponent->GetComponentRotation(), true, NULL, MOVECOMP_IgnoreBases);

		// @TODO FIXMESTEVE - really correct would be recurse (with cap)
		if (bMoveWasBlocked)
		{
			LiftStartLocation = EncroachComponent->GetComponentLocation();
			EncroachComponent->MoveComponent(NewLocation - EncroachComponent->GetComponentLocation(), EncroachComponent->GetComponentRotation(), true, NULL, MOVECOMP_IgnoreBases);
		}
	}
}

void AUTLift::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (EncroachComponent != NULL && DeltaTime > 0.f)
	{
		FVector NewLoc = EncroachComponent->GetComponentLocation();
		LiftVelocity = (NewLoc - TickLocation) / DeltaTime;
		EncroachComponent->ComponentVelocity = LiftVelocity;
		TickLocation = NewLoc;
	}
}

FVector AUTLift::GetVelocity() const
{
	return LiftVelocity;
}

void AUTLift::UpdateCurrentlyBasedCharacters(AUTCharacter* BasedCharacter)
{
	if (BasedCharacter && !BasedCharacter->IsDead() && !BasedCharacter->IsPendingKillPending())
	{
		BasedCharacters.AddUnique(BasedCharacter);
	}
}

bool AUTLift::HasBasedCharacters()
{
	if (BasedCharacters.Num() == 0)
	{
		return false;
	}

	// verify BasedCharacters entries are valid
	for (int32 i = 0; i<BasedCharacters.Num(); i++)
	{
		AUTCharacter* Next = BasedCharacters[i];
		if ((Next == NULL) || Next->IsPendingKillPending() || Next->IsDead() || (Next->GetMovementBaseActor(Next) != this))
		{
			BasedCharacters.RemoveAt(i, 1);
			i--;
		}
	}

	return (BasedCharacters.Num() > 0);
}


void AUTLift::GetNavigationData(struct FNavigationRelevantData& Data) const
{
	UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
	if (NavSys != NULL && NavSys->GetNavOctree() != NULL && NavSys->GetNavOctree()->ComponentExportDelegate.IsBound())
	{
		// FIXME: navmesh only supports one set of data per object, so we can only do one stop!
		//		this is a workaround for navmesh not supporting movers anyway, so hopefully we will be able to eventually just delete this instead of fixing
		TArray<FVector> Stops = GetStops();
		if (Stops.Num() == 0)
		{
			UE_LOG(UT, Error, TEXT("%s didn't return any stops! Please implement GetStops() in the blueprint"), *GetName());
		}
		for (const FVector& NextStop : Stops)
		{
			FVector Offset = NextStop - GetActorLocation();
			if (!Offset.IsNearlyZero())
			{
				// temporarily move the component for export
				FScopedMovementUpdate TempMove(EncroachComponent);
				// scale down the component for export to try to prevent the exported geometry from connecting to normal areas
				FVector SavedScale = EncroachComponent->GetComponentScale();
				EncroachComponent->SetWorldScale3D(SavedScale * FVector(NavmeshScale, NavmeshScale, 0.1f));
				EncroachComponent->SetWorldLocation(NextStop, false);
				NavSys->GetNavOctree()->ComponentExportDelegate.Execute(EncroachComponent, Data);
				EncroachComponent->SetWorldScale3D(SavedScale);
				TempMove.RevertMove();
				break; // see above
			}
		}
	}
}
FBox AUTLift::GetNavigationBounds() const
{
	FBox BaseBounds = GetComponentsBoundingBox();
	FBox TotalBounds = BaseBounds;

	TArray<FVector> Stops = GetStops();
	for (const FVector& NextStop : Stops)
	{
		FVector Offset = NextStop - GetActorLocation();
		if (!Offset.IsNearlyZero())
		{
			TotalBounds += BaseBounds.TransformBy(FTranslationMatrix(Offset));
		}
	}
	return TotalBounds;
}

void AUTLift::AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData)
{
	if (EncroachComponent != NULL)
	{
		const FVector MyLoc = GetActorLocation();
		const FVector LiftCenter = GetComponentsBoundingBox().GetCenter();
		FHitResult TopHit;
		if (ActorLineTraceSingle(TopHit, LiftCenter + FVector(0.0f, 0.0f, 10000.0f), LiftCenter - FVector(0.0f, 0.0f, 10000.0f), ECC_Pawn, FCollisionQueryParams()))
		{
			float ZOffset = TopHit.Location.Z + NavData->AgentHeight * 0.25f - MyLoc.Z;
			// figure out some offsets to check for exit locations at each start
			TArray<FVector> ExitOffsets;
			FVector RelDirs[] = { FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f) };
			FRotationMatrix ActorRotMat(GetActorRotation());
			for (int32 i = 0; i < ARRAY_COUNT(RelDirs); i++)
			{
				FVector WorldDir = ActorRotMat.TransformVector(RelDirs[i]);
				FHitResult Hit;
				if (ActorLineTraceSingle(Hit, EncroachComponent->Bounds.Origin + WorldDir * 10000.0f, EncroachComponent->Bounds.Origin - WorldDir * 10000.0f, ECC_Pawn, FCollisionQueryParams()))
				{
					FVector PotentialOffset = (Hit.Location + WorldDir * NavData->AgentRadius * 3.0f) - MyLoc;
					PotentialOffset.Z = ZOffset;
					ExitOffsets.Add(PotentialOffset);
				}
			}

			TSubclassOf<ACharacter> ScoutType = NavData->ScoutClass;
			if (ScoutType == NULL)
			{
				ScoutType = AUTCharacter::StaticClass();
			}

			TArray<FVector> Stops = GetStops();
			for (const FVector& NextStop : Stops)
			{
				for (const FVector& Offset : ExitOffsets)
				{
					FVector ExitLoc = NextStop + Offset;
					FVector AdjustedLoc = ExitLoc;
					// check that we can place the exit loc there without XY adjustments and trace to there from lift center is clear
					// TODO: using Scout here only because FindTeleportSpot()/EncroachingWorldGeometry() won't accept standalone collision shapes
					if ( GetWorld()->FindTeleportSpot(ScoutType.GetDefaultObject(), AdjustedLoc, FRotator::ZeroRotator) && (ExitLoc - AdjustedLoc).Size2D() < 1.0f &&
						!GetWorld()->SweepTestByChannel(NextStop + FVector(0.0f, 0.0f, ZOffset + NavData->AgentHeight * 0.5f), AdjustedLoc + FVector(0.0f, 0.0f, NavData->AgentHeight * 0.5f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(NavData->AgentRadius, NavData->AgentHeight * 0.25f), FCollisionQueryParams()) )
					{
						// make sure to account for differences in Z between test capsule and nav height that it's expecting for poly finding
						ExitLoc = AdjustedLoc - FVector(0.0f, 0.0f, ScoutType.GetDefaultObject()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - NavData->AgentHeight * 0.5f);
						// if the LD placed an exit nearby then don't place another
						bool bManualExit = false;
						for (TActorIterator<AUTLiftExit> It(GetWorld()); It; ++It)
						{
							if (It->MyLift == this && FMath::Abs<float>(It->GetActorLocation().Z - ExitLoc.Z) < NavData->AgentHeight * 2.0f && (It->GetActorLocation() - ExitLoc).Size2D() < 1024.0f)
							{
								bManualExit = true;
								break;
							}
						}

						if (!bManualExit)
						{
							AUTLiftExit::AddLiftPathsShared(ExitLoc, this, false, false, NavData);
						}
					}
				}
			}
		}
	}
}