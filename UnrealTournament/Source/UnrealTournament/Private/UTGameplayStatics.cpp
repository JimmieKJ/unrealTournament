// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameplayStatics.h"

void UUTGameplayStatics::UTPlaySound(UWorld* TheWorld, USoundBase* TheSound, AActor* SourceActor, ESoundReplicationType RepType, bool bStopWhenOwnerDestroyed, const FVector& SoundLoc, AUTPlayerController* AmpedListener)
{
	if (TheSound != NULL && !GExitPurge)
	{
		if (SourceActor == NULL && SoundLoc.IsZero())
		{
			UE_LOG(UT, Warning, TEXT("UTPlaySound(): No source (SourceActor == None and SoundLoc not specified)"));
		}
		else if (SourceActor == NULL && TheWorld == NULL)
		{
			UE_LOG(UT, Warning, TEXT("UTPlaySound(): Missing SourceActor"));
		}
		else if (TheWorld == NULL && SourceActor->GetWorld() == NULL)
		{
			UE_LOG(UT, Warning, TEXT("UTPlaySound(): Source isn't in a world"));
		}
		else
		{
			if (TheWorld == NULL)
			{
				TheWorld = SourceActor->GetWorld();
			}
			if (RepType >= SRT_MAX)
			{
				UE_LOG(UT, Warning, TEXT("UTPlaySound(): Unexpected RepType"));
				RepType = SRT_All;
			}

			const FVector& SourceLoc = !SoundLoc.IsZero() ? SoundLoc : SourceActor->GetActorLocation();

			if (TheWorld->GetNetMode() != NM_Standalone && TheWorld->GetNetDriver() != NULL)
			{
				APlayerController* TopOwner = NULL;
				for (AActor* TestActor = SourceActor; TestActor != NULL && TopOwner == NULL; TestActor = TestActor->GetOwner())
				{
					TopOwner = Cast<APlayerController>(TestActor);
				}

				for (int32 i = 0; i < TheWorld->GetNetDriver()->ClientConnections.Num(); i++)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(TheWorld->GetNetDriver()->ClientConnections[i]->OwningActor);
					if (PC != NULL)
					{
						bool bShouldReplicate;
						switch (RepType)
						{
						case SRT_All:
							bShouldReplicate = true;
							break;
						case SRT_AllButOwner:
							bShouldReplicate = PC != TopOwner;
							break;
						case SRT_IfSourceNotReplicated:
							bShouldReplicate = TheWorld->GetNetDriver()->ClientConnections[i]->ActorChannels.Find(SourceActor) == NULL;
							break;
						case SRT_None:
							bShouldReplicate = false;
							break;
						default:
							// should be impossible
							UE_LOG(UT, Warning, TEXT("UTPlaySound(): Unhandled sound replication type %i"), int32(RepType));
							bShouldReplicate = true;
							break;
						}

						if (bShouldReplicate)
						{
							PC->HearSound(TheSound, SourceActor, SourceLoc, bStopWhenOwnerDestroyed, AmpedListener == PC);
						}
					}
				}
			}

			for (FLocalPlayerIterator It(GEngine, TheWorld); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
				if (PC != NULL && PC->IsLocalPlayerController())
				{
					PC->HearSound(TheSound, SourceActor, SourceLoc, bStopWhenOwnerDestroyed, AmpedListener == PC);
				}
			}
		}
	}
}

/** largely copied from GameplayStatics.cpp, with mods to use our trace channel and better handling if we don't get a hit on the target */
static bool ComponentIsVisibleFrom(UPrimitiveComponent* VictimComp, FVector const& Origin, AActor const* IgnoredActor, const TArray<AActor*>& IgnoreActors, FHitResult& OutHitResult)
{
	static FName NAME_ComponentIsVisibleFrom = FName(TEXT("ComponentIsVisibleFrom"));
	FCollisionQueryParams LineParams(NAME_ComponentIsVisibleFrom, true, IgnoredActor);
	LineParams.AddIgnoredActors(IgnoreActors);

	// Do a trace from origin to middle of box
	UWorld* World = VictimComp->GetWorld();

	FVector const TraceEnd = VictimComp->Bounds.Origin;
	FVector TraceStart = Origin;
	if (Origin == TraceEnd)
	{
		// tiny nudge so LineTraceSingle doesn't early out with no hits
		TraceStart.Z += 0.01f;
	}
	bool const bHadBlockingHit = World->LineTraceSingle(OutHitResult, TraceStart, TraceEnd, COLLISION_TRACE_WEAPON, LineParams);

	// If there was a blocking hit, it will be the last one
	if (bHadBlockingHit)
	{
		if (OutHitResult.Component == VictimComp)
		{
			// if blocking hit was the victim component, it is visible
			return true;
		}
		else
		{
			// if we hit something else blocking, it's not
			UE_LOG(LogDamage, Log, TEXT("Radial Damage to %s blocked by %s (%s)"), *GetNameSafe(VictimComp), *GetNameSafe(OutHitResult.GetActor()), *GetNameSafe(OutHitResult.Component.Get()));
			return false;
		}
	}

	// didn't hit anything, including the victim component; try a component only trace to get hit information
	if (!VictimComp->LineTraceComponent(OutHitResult, TraceStart, TraceEnd, LineParams))
	{
		FVector FakeHitLoc = VictimComp->GetComponentLocation();
		OutHitResult = FHitResult(VictimComp->GetOwner(), VictimComp, FakeHitLoc, (Origin - FakeHitLoc).SafeNormal());
	}
	return true;
}
bool UUTGameplayStatics::UTHurtRadius(UObject* WorldContextObject, float BaseDamage, float MinimumDamage, float BaseMomentumMag, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff, TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController)
{
	static FName NAME_ApplyRadialDamage = FName(TEXT("ApplyRadialDamage"));
	FCollisionQueryParams SphereParams(NAME_ApplyRadialDamage, false, DamageCauser);

	SphereParams.AddIgnoredActors(IgnoreActors);
	if (DamageCauser != NULL)
	{
		SphereParams.IgnoreActors.Add(DamageCauser->GetUniqueID());
	}

	// query scene to see what we hit
	TArray<FOverlapResult> Overlaps;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	World->OverlapMulti(Overlaps, Origin, FQuat::Identity, COLLISION_TRACE_WEAPON,  FCollisionShape::MakeSphere(DamageOuterRadius), SphereParams);

	// collate into per-actor list of hit components
	TMap< AActor*, TArray<FHitResult> > OverlapComponentMap;
	for (int32 Idx = 0; Idx < Overlaps.Num(); ++Idx)
	{
		FOverlapResult const& Overlap = Overlaps[Idx];
		AActor* const OverlapActor = Overlap.GetActor();

		if (OverlapActor != NULL && OverlapActor->bCanBeDamaged && Overlap.Component.IsValid())
		{
			FHitResult Hit;
			if (ComponentIsVisibleFrom(Overlap.Component.Get(), Origin, DamageCauser, IgnoreActors, Hit))
			{
				TArray<FHitResult>& HitList = OverlapComponentMap.FindOrAdd(OverlapActor);
				HitList.Add(Hit);
			}
		}
	}

	// make sure we have a good damage type
	TSubclassOf<UDamageType> const ValidDamageTypeClass = (DamageTypeClass == NULL) ? TSubclassOf<UDamageType>(UDamageType::StaticClass()) : DamageTypeClass;

	bool bAppliedDamage = false;

	// call damage function on each affected actors
	for (TMap<AActor*, TArray<FHitResult> >::TIterator It(OverlapComponentMap); It; ++It)
	{
		AActor* const Victim = It.Key();
		TArray<FHitResult> const& ComponentHits = It.Value();

		FUTRadialDamageEvent DmgEvent;
		DmgEvent.DamageTypeClass = ValidDamageTypeClass;
		DmgEvent.ComponentHits = ComponentHits;
		DmgEvent.Origin = Origin;
		DmgEvent.Params = FRadialDamageParams(BaseDamage, MinimumDamage, DamageInnerRadius, DamageOuterRadius, DamageFalloff);
		DmgEvent.BaseMomentumMag = BaseMomentumMag;

		Victim->TakeDamage(BaseDamage, DmgEvent, InstigatedByController, DamageCauser);

		bAppliedDamage = true;
	}

	return bAppliedDamage;
}

APawn* UUTGameplayStatics::PickBestAimTarget(AController* AskingC, FVector StartLoc, FVector FireDir, float MinAim, float MaxRange, TSubclassOf<APawn> TargetClass, float* BestAim, float* BestDist)
{
	if (AskingC == NULL)
	{
		UE_LOG(UT, Warning, TEXT("PickBestAimTarget(): AskingC == NULL"));
		return NULL;
	}
	else if (AskingC->GetNetMode() == NM_Client)
	{
		UE_LOG(UT, Warning, TEXT("PickBestAimTarget(): Only callable on server"));
		return NULL;
	}
	else
	{
		if (TargetClass == NULL)
		{
			TargetClass = APawn::StaticClass();
		}
		const float MaxRangeSquared = FMath::Square(MaxRange);
		const float VerticalMinAim = MinAim * 3.f - 2.f;
		float LocalBestAim;
		float LocalBestDist;
		if (BestAim == NULL)
		{
			BestAim = &LocalBestAim;
		}
		if (BestDist == NULL)
		{
			BestDist = &LocalBestDist;
		}
		(*BestDist) = FLT_MAX;
		(*BestAim) = MinAim;
		APawn* BestTarget = NULL;
		FCollisionQueryParams TraceParams(FName(TEXT("PickBestAimTarget")), false);
		UWorld* TheWorld = AskingC->GetWorld();
		for (FConstControllerIterator It = TheWorld->GetControllerIterator(); It; ++It)
		{
			APawn* P = It->Get()->GetPawn();
			if (P != NULL && !P->bTearOff)
			{
				/* TODO:
				if (!P->IsRootComponentCollisionRegistered())
				{
					// perhaps target vehicle this pawn is based on instead
					NewTarget = NewTarget->GetVehicleBase();
					if (!NewTarget || NewTarget->Controller)
					{
						continue;
					}
				}
				*/

				if (P->GetClass()->IsChildOf(TargetClass))
				{
					AUTGameState* GS = TheWorld->GetGameState<AUTGameState>();
					if (GS == NULL || !GS->OnSameTeam(AskingC, P))
					{
						// check passed in constraints
						const FVector AimDir = P->GetActorLocation() - StartLoc;
						float TestAim = FireDir | AimDir;
						if (TestAim > 0.0f)
						{
							float FireDist = AimDir.SizeSquared();
							if (FireDist < MaxRangeSquared)
							{
								FireDist = FMath::Sqrt(FireDist);
								TestAim /= FireDist;
								bool bPassedAimCheck = (TestAim > *BestAim);
								// if no target yet, be more liberal about up/down error (more vertical autoaim help)
								if (!bPassedAimCheck && BestTarget == NULL && TestAim > VerticalMinAim)
								{
									FVector FireDir2D = FireDir;
									FireDir2D.Z = 0;
									FireDir2D.Normalize();
									float TestAim2D = FireDir2D | AimDir;
									TestAim2D = TestAim2D / FireDist;
									bPassedAimCheck = (TestAim2D > *BestAim);
								}
								if (bPassedAimCheck)
								{
									// trace to head and center
									bool bHit = TheWorld->LineTraceTest(StartLoc, P->GetActorLocation() + FVector(0.0f, 0.0f, P->BaseEyeHeight), ECC_Visibility, TraceParams);
									if (bHit)
									{
										bHit = TheWorld->LineTraceTest(StartLoc, P->GetActorLocation(), ECC_Visibility, TraceParams);
									}
									if (!bHit)
									{
										BestTarget = P;
										(*BestAim) = TestAim;
										(*BestDist) = FireDist;
									}
								}
							}
						}
					}
				}
			}
		}

		return BestTarget;
	}
}