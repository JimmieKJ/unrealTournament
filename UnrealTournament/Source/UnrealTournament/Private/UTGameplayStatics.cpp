// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameplayStatics.h"
#include "Runtime/Engine/Classes/Engine/DemoNetDriver.h"

void UUTGameplayStatics::UTPlaySound(UWorld* TheWorld, USoundBase* TheSound, AActor* SourceActor, ESoundReplicationType RepType, bool bStopWhenOwnerDestroyed, const FVector& SoundLoc, AUTPlayerController* AmpedListener, APawn* Instigator, bool bNotifyAI)
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
							bShouldReplicate = SourceActor == NULL || TheWorld->GetNetDriver()->ClientConnections[i]->ActorChannels.Find(SourceActor) == NULL;
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
			// write into demo if there is one
			if (TheWorld->DemoNetDriver != NULL && TheWorld->DemoNetDriver->ServerConnection == NULL && RepType != SRT_None && RepType != SRT_IfSourceNotReplicated)
			{
				// TODO: engine doesn't set this on record for some reason
				//AUTPlayerController* PC = Cast<AUTPlayerController>(TheWorld->DemoNetDriver->SpectatorController);
				AUTPlayerController* PC = (TheWorld->DemoNetDriver->ClientConnections.Num() > 0) ? Cast<AUTPlayerController>(TheWorld->DemoNetDriver->ClientConnections[0]->PlayerController) : NULL;
				if (PC != NULL)
				{
					PC->ClientHearSound(TheSound, SourceActor, (SourceActor != NULL && SourceActor->GetActorLocation() == SourceLoc) ? FVector::ZeroVector : SourceLoc, bStopWhenOwnerDestroyed, false, false);
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

			if (bNotifyAI)
			{
				if (Instigator == NULL)
				{
					Instigator = Cast<APawn>(SourceActor);
				}
				if (Instigator != NULL)
				{
					// note: all sound attenuation treated as a sphere
					float Radius = TheSound->GetMaxAudibleDistance();
					const FAttenuationSettings* Settings = TheSound->GetAttenuationSettingsToApply();
					if (Settings != NULL)
					{
						Radius = FMath::Max<float>(Radius, Settings->GetMaxDimension());
					}
					for (FConstControllerIterator It = TheWorld->GetControllerIterator(); It; ++It)
					{
						if (It->IsValid())
						{
							AUTBot* B = Cast<AUTBot>(It->Get());
							if (B != NULL && B->GetPawn() != NULL && B->GetPawn() != Instigator && (Radius <= 0.0f || Radius > (SourceLoc - B->GetPawn()->GetActorLocation()).Size() * B->HearingRadiusMult))
							{
								B->HearSound(Instigator, SourceLoc, Radius);
							}
						}
					}
				}
			}
		}
	}
}

float UUTGameplayStatics::GetGravityZ(UObject* WorldContextObject, const FVector& TestLoc)
{
	UWorld* World = (WorldContextObject != NULL) ? WorldContextObject->GetWorld() : NULL;
	if (World == NULL)
	{
		return GetDefault<UPhysicsSettings>()->DefaultGravityZ;
	}
	else if (TestLoc.IsZero())
	{
		return World->GetDefaultGravityZ();
	}
	else
	{
		APhysicsVolume* Volume = FindPhysicsVolume(World, TestLoc, FCollisionShape::MakeSphere(0.0f));
		return (Volume != NULL) ? Volume->GetGravityZ() : World->GetDefaultGravityZ();
	}
}

/** largely copied from GameplayStatics.cpp, with mods to use our trace channel and better handling if we don't get a hit on the target */
static bool ComponentIsVisibleFrom(UPrimitiveComponent* VictimComp, FVector const& Origin, AActor const* IgnoredActor, FHitResult& OutHitResult)
{
	static FName NAME_ComponentIsVisibleFrom = FName(TEXT("ComponentIsVisibleFrom"));
	FCollisionQueryParams LineParams(NAME_ComponentIsVisibleFrom, true, IgnoredActor);
	FCollisionResponseParams ResponseParams = FCollisionResponseParams::DefaultResponseParam;
	ResponseParams.CollisionResponse.Pawn = ECR_Ignore; // we don't want Pawns to block other Pawns from being hit also

	// Do a trace from origin to middle of box
	UWorld* World = VictimComp->GetWorld();

	FVector const TraceEnd = VictimComp->Bounds.Origin;
	FVector TraceStart = Origin;
	if (Origin == TraceEnd)
	{
		// tiny nudge so LineTraceSingle doesn't early out with no hits
		TraceStart.Z += 0.01f;
	}
	bool const bHadBlockingHit = World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, COLLISION_TRACE_WEAPON, LineParams, ResponseParams);

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
	// LineTraceComponent() assumes Visibility channel so we have to force that on...
	ECollisionResponse SavedVisResponse = VictimComp->GetCollisionResponseToChannel(ECC_Visibility);
	VictimComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	if (!VictimComp->LineTraceComponent(OutHitResult, TraceStart, TraceEnd, LineParams))
	{
		FVector FakeHitLoc = VictimComp->GetComponentLocation();
		OutHitResult = FHitResult(VictimComp->GetOwner(), VictimComp, FakeHitLoc, (Origin - FakeHitLoc).GetSafeNormal());
	}
	VictimComp->SetCollisionResponseToChannel(ECC_Visibility, SavedVisResponse);
	return true;
}
bool UUTGameplayStatics::UTHurtRadius( UObject* WorldContextObject, float BaseDamage, float MinimumDamage, float BaseMomentumMag, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff,
									   TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController, AController* FFInstigatedBy, TSubclassOf<UDamageType> FFDamageType, float CollisionFreeRadius )
{
	static FName NAME_ApplyRadialDamage = FName(TEXT("ApplyRadialDamage"));
	FCollisionQueryParams SphereParams(NAME_ApplyRadialDamage, true, DamageCauser);

	SphereParams.AddIgnoredActors(IgnoreActors);
	if (DamageCauser != NULL)
	{
		SphereParams.AddIgnoredActor(DamageCauser);
	}

	// query scene to see what we hit
	TArray<FOverlapResult> Overlaps;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	AUTGameState* GS = World->GetGameState<AUTGameState>();
	World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeSphere(DamageOuterRadius), SphereParams);

	// collate into per-actor list of hit components
	TMap< AActor*, TArray<FHitResult> > OverlapComponentMap;
	for (int32 Idx = 0; Idx < Overlaps.Num(); ++Idx)
	{
		FOverlapResult const& Overlap = Overlaps[Idx];
		AActor* const OverlapActor = Overlap.GetActor();

		if (OverlapActor != NULL && OverlapActor->bCanBeDamaged && Overlap.Component.IsValid())
		{
			FHitResult Hit(OverlapActor, Overlap.Component.Get(), OverlapActor->GetActorLocation(), FVector(0,0,1.f));
			if ((OverlapActor->GetActorLocation() - Origin).Size() <= CollisionFreeRadius || 
				ComponentIsVisibleFrom(Overlap.Component.Get(), Origin, DamageCauser, Hit))
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
		APawn* const VictimPawn = Cast<APawn>(Victim);
		TArray<FHitResult> const& ComponentHits = It.Value();

		FUTRadialDamageEvent DmgEvent;
		DmgEvent.DamageTypeClass = ValidDamageTypeClass;
		DmgEvent.ComponentHits = ComponentHits;
		DmgEvent.Origin = Origin;
		DmgEvent.Params = FRadialDamageParams(BaseDamage, MinimumDamage, DamageInnerRadius, DamageOuterRadius, DamageFalloff);
		DmgEvent.BaseMomentumMag = BaseMomentumMag;

		// if it's friendly fire, possibly redirect to alternate Controller for damage credit
		AController* ResolvedInstigator = InstigatedByController;
		if ( FFInstigatedBy != NULL && InstigatedByController != NULL && FFInstigatedBy != InstigatedByController &&
			((GS != NULL && GS->OnSameTeam(InstigatedByController, Victim)) || InstigatedByController == Victim || (VictimPawn != NULL && VictimPawn->Controller == InstigatedByController)) )
		{
			ResolvedInstigator = FFInstigatedBy;
			if (FFDamageType != NULL)
			{
				DmgEvent.DamageTypeClass = FFDamageType;
			}
		}

		Victim->TakeDamage(BaseDamage, DmgEvent, ResolvedInstigator, DamageCauser);

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
			if (P != NULL && !P->bTearOff && It->Get() != AskingC)
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
									bool bHit = TheWorld->LineTraceTestByChannel(StartLoc, P->GetActorLocation() + FVector(0.0f, 0.0f, P->BaseEyeHeight), ECC_Visibility, TraceParams);
									if (bHit)
									{
										bHit = TheWorld->LineTraceTestByChannel(StartLoc, P->GetActorLocation(), ECC_Visibility, TraceParams);
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

bool UUTGameplayStatics::UTSuggestProjectileVelocity(UObject* WorldContextObject, FVector& TossVelocity, const FVector& StartLoc, const FVector& EndLoc, AActor* TargetActor, float ZOvershootTolerance, float TossSpeed, float CollisionRadius, float OverrideGravityZ, int32 MaxSubdivisions, ESuggestProjVelocityTraceOption::Type TraceOption)
{
	// if we're not tracing, then our special code isn't going to do anything of value
	if (TraceOption == ESuggestProjVelocityTraceOption::DoNotTrace)
	{
		return UGameplayStatics::SuggestProjectileVelocity(WorldContextObject, TossVelocity, StartLoc, EndLoc, TossSpeed, false, CollisionRadius, OverrideGravityZ, TraceOption);
	}

	const FVector FlightDelta = EndLoc - StartLoc;
	const FVector DirXY = FlightDelta.GetSafeNormal2D();
	const float DeltaXY = FlightDelta.Size2D();
	const float DeltaZ = FlightDelta.Z;

	const float TossSpeedSq = FMath::Square(TossSpeed);

	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	const float GravityZ = (OverrideGravityZ != 0.f) ? -OverrideGravityZ : -World->GetGravityZ();

	// v^4 - g*(g*x^2 + 2*y*v^2)
	const float InsideTheSqrt = FMath::Square(TossSpeedSq) - GravityZ * ((GravityZ * FMath::Square(DeltaXY)) + (2.f * DeltaZ * TossSpeedSq));
	if (InsideTheSqrt < 0.f)
	{
		// sqrt will be imaginary, therefore no solutions
		return false;
	}

	// if we got here, there are 2 solutions: one high-angle and one low-angle.

	const float SqrtPart = FMath::Sqrt(InsideTheSqrt);

	// this is the tangent of the firing angle for the first (+) solution
	float TanSolutionLowAngle = (TossSpeedSq + SqrtPart) / (GravityZ * DeltaXY);
	// this is the tangent of the firing angle for the second (-) solution
	float TanSolutionHighAngle = (TossSpeedSq - SqrtPart) / (GravityZ * DeltaXY);

	if (FMath::Square(TanSolutionLowAngle) > FMath::Square(TanSolutionHighAngle))
	{
		Exchange<float>(TanSolutionLowAngle, TanSolutionHighAngle);
	}

	// create a list of all the arcs we want to try

	const int32 TotalArcs = MaxSubdivisions + 2;

	TArray<float> PrioritizedSolutionsMagXYSq;
	PrioritizedSolutionsMagXYSq.Reserve(TotalArcs);
	TArray<float> PrioritizedSolutionZSign;
	PrioritizedSolutionZSign.Reserve(TotalArcs);

	// mag in the XY dir = sqrt( TossSpeedSq / (TanSolutionAngle^2 + 1) );
	PrioritizedSolutionsMagXYSq.Add(TossSpeedSq / (FMath::Square(TanSolutionLowAngle) + 1.f));
	PrioritizedSolutionZSign.Add(FMath::Sign(TanSolutionLowAngle));
	for (int32 i = 1; i < TotalArcs - 1; i++)
	{
		float AdjustedAngle = FMath::Lerp<float>(TanSolutionLowAngle, TanSolutionHighAngle, (1.0f / (TotalArcs - 1)) * i);
		const float MagXYSq = TossSpeedSq / (FMath::Square(AdjustedAngle) + 1.f);
		const float MagXY = FMath::Sqrt(MagXYSq);
		const float ZSign = FMath::Sign(AdjustedAngle);
		const float ZSpeed = FMath::Sqrt(TossSpeedSq - MagXYSq) * ZSign;
		// check that this intermediate solution is within the Z tolerance
		if (StartLoc.Z + ZSpeed * (DeltaXY / MagXY) <= EndLoc.Z + ZOvershootTolerance)
		{
			PrioritizedSolutionsMagXYSq.Add(MagXYSq);
			PrioritizedSolutionZSign.Add(ZSign);
		}
	}
	PrioritizedSolutionsMagXYSq.Add(TossSpeedSq / (FMath::Square(TanSolutionHighAngle) + 1.f));
	PrioritizedSolutionZSign.Add(FMath::Sign(TanSolutionHighAngle));
	
	static const FName NAME_SuggestProjVelTrace = FName(TEXT("SuggestProjVelTrace"));
	FCollisionQueryParams QueryParams(NAME_SuggestProjVelTrace, true, TargetActor);
	FCollisionResponseParams ResponseParam = FCollisionResponseParams::DefaultResponseParam;
	ResponseParam.CollisionResponse.Pawn = ECR_Ignore;

	// try solutions low to high
	for (int32 CurrentSolutionIdx = 0; CurrentSolutionIdx < PrioritizedSolutionsMagXYSq.Num(); CurrentSolutionIdx++)
	{
		const float MagXY = FMath::Sqrt(PrioritizedSolutionsMagXYSq[CurrentSolutionIdx]);
		const float MagZ = FMath::Sqrt(TossSpeedSq - PrioritizedSolutionsMagXYSq[CurrentSolutionIdx]);		// pythagorean
		const float ZSign = PrioritizedSolutionZSign[CurrentSolutionIdx];

		const FVector ProjVelocity = (DirXY * MagXY) + (FVector::UpVector * MagZ * ZSign);

		// iterate along the arc, doing stepwise traces
		// TODO: we could potentially optimize later traces at the cost of a relatively small amount of accuracy by tracing from start point to the Step that previously failed
		bool bFailedTrace = false;
		static const float StepSize = 0.125f;
		FVector TraceStart = StartLoc;
		for (float Step = 0.f; Step < 1.f; Step += StepSize)
		{
			const float TimeInFlight = (Step + StepSize) * DeltaXY / MagXY;

			// d = vt + .5 a t^2
			const FVector TraceEnd = StartLoc + ProjVelocity * TimeInFlight + FVector(0.f, 0.f, 0.5f * -GravityZ * FMath::Square(TimeInFlight) - CollisionRadius);

			if (TraceOption == ESuggestProjVelocityTraceOption::OnlyTraceWhileAsceding && TraceEnd.Z < TraceStart.Z)
			{
				// falling, we are done tracing
				break;
			}
			else
			{
				// note: this will automatically fall back to line test if radius is small enough
				if (World->SweepTestByChannel(TraceStart, TraceEnd, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(CollisionRadius), QueryParams, ResponseParam))
				{
					// hit something, failed
					bFailedTrace = true;
					break;
				}

			}

			// advance
			TraceStart = TraceEnd;
		}

		if (!bFailedTrace)
		{
			// passes all traces along the arc, we have a valid solution and can be done
			TossVelocity = ProjVelocity;
			return true;
		}
	}

	return false;
}

APlayerController* UUTGameplayStatics::GetLocalPlayerController(UObject* WorldContextObject, int32 PlayerIndex)
{
	UWorld* World = (WorldContextObject != NULL) ? WorldContextObject->GetWorld() : NULL;
	if (World == NULL)
	{
		return NULL;
	}
	else
	{
		for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
		{
			PlayerIndex--;
			if (PlayerIndex < 0)
			{
				return It->PlayerController;
			}
		}
		return NULL;
	}
}

void UUTGameplayStatics::SaveConfig(UObject* Obj)
{
	UClass* Cls = Cast<UClass>(Obj);
	if (Cls != NULL)
	{
		Cls->GetDefaultObject()->SaveConfig();
	}
	else
	{
		Obj->SaveConfig();
	}
	GConfig->Flush(false);
}

class UAudioComponent* UUTGameplayStatics::PlaySoundTeamAdjusted(USoundCue* SoundToPlay, AActor* SoundInstigator, AActor* SoundTarget, bool Attached)
{
	if (!SoundToPlay || !SoundTarget)
	{
		return nullptr;
	}

	//Don't play sounds on dedicated servers
	ENetMode CurNetMode = (SoundTarget->GetWorld()) ? GEngine->GetNetMode(SoundTarget->GetWorld()) : NM_Standalone;
	if (SoundTarget->GetWorld() && (GEngine->GetNetMode(SoundTarget->GetWorld()) == NM_DedicatedServer))
	{
		return nullptr;
	}

	UAudioComponent* AudioComponent = FAudioDevice::CreateComponent(SoundToPlay, SoundTarget->GetWorld(), SoundTarget, false, false);
	if (AudioComponent)
	{
		const bool bIsInGameWorld = AudioComponent->GetWorld()->IsGameWorld();

		if (Attached)
		{
			AudioComponent->AttachTo(SoundTarget->GetRootComponent());
		}
		else
		{
			AudioComponent->SetWorldLocation(SoundTarget->GetActorLocation());
		}

		AudioComponent->SetVolumeMultiplier(1.0f);
		AudioComponent->SetPitchMultiplier(1.0f);
		AudioComponent->bAllowSpatialization = bIsInGameWorld;
		AudioComponent->bIsUISound = !bIsInGameWorld;
		AudioComponent->bAutoDestroy = true;
		AudioComponent->SubtitlePriority = 10000.f; // Fixme: pass in? Do we want that exposed to blueprints though?
		AudioComponent->AttenuationSettings = nullptr;

		AssignTeamAdjustmentValue(AudioComponent, SoundInstigator ? SoundInstigator : SoundTarget);
		AudioComponent->Play(0.0f);
	}

	return AudioComponent;
}

void UUTGameplayStatics::AssignTeamAdjustmentValue(UAudioComponent* AudioComponent, AActor* SoundInstigator)
{
	check(AudioComponent);
	check(SoundInstigator);

	AUTCharacter* UTCharInstigator = Cast<AUTCharacter>(SoundInstigator);
	if (UTCharInstigator && UTCharInstigator->IsLocallyControlled())
	{
		AudioComponent->SetIntParameter(FName("ListenerStyle"), 0);
	}
	else
	{
		bool bOnSameTeam = false;
		AUTPlayerController* const LocalPC = Cast<AUTPlayerController>(GEngine->GetFirstLocalPlayerController(SoundInstigator->GetWorld()));
		uint8 InstigatorTeam = 255;

		const IUTTeamInterface* TeamInterface = Cast<IUTTeamInterface>(SoundInstigator);
		if (TeamInterface != nullptr)
		{
			InstigatorTeam = TeamInterface->GetTeamNum();
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("AssignTeamAdjustmentValue called for invalid SoundInstigator"));
		}

		if (LocalPC && InstigatorTeam != 255 && InstigatorTeam == LocalPC->GetTeamNum())
		{
			AudioComponent->SetIntParameter(FName("ListenerStyle"), 1);
		}
		else
		{
			AudioComponent->SetIntParameter(FName("ListenerStyle"), 2);
		}
	}
}

bool UUTGameplayStatics::HasTokenBeenPickedUpBefore(UObject* WorldContextObject, FName TokenUniqueID)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP && LP->GetProfileSettings())
		{
			return LP->GetProfileSettings()->HasTokenBeenPickedUpBefore(TokenUniqueID);
		}
	}

	return false;
}

void UUTGameplayStatics::TokenPickedUp(UObject* WorldContextObject, FName TokenUniqueID)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP && LP->GetProfileSettings())
		{
			LP->GetProfileSettings()->TokenPickedUp(TokenUniqueID);
		}
	}
}

void UUTGameplayStatics::TokenRevoke(UObject* WorldContextObject, FName TokenUniqueID)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP && LP->GetProfileSettings())
		{
			LP->GetProfileSettings()->TokenRevoke(TokenUniqueID);
		}
	}
}

void UUTGameplayStatics::TokensCommit(UObject* WorldContextObject)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP && LP->GetProfileSettings())
		{
			LP->GetProfileSettings()->TokensCommit();
		}
	}
}

void UUTGameplayStatics::TokensReset(UObject* WorldContextObject)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP && LP->GetProfileSettings())
		{
			LP->GetProfileSettings()->TokensReset();
		}
	}
}

void UUTGameplayStatics::SetBestTime(UObject* WorldContextObject, FName TimingSection, float InBestTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP && LP->GetProfileSettings())
		{
			LP->GetProfileSettings()->SetBestTime(TimingSection, InBestTime);
		}
	}
}

bool UUTGameplayStatics::GetBestTime(UObject* WorldContextObject, FName TimingSection, float& OutBestTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP && LP->GetProfileSettings())
		{
			return LP->GetProfileSettings()->GetBestTime(TimingSection, OutBestTime);
		}
	}

	return false;
}
