// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "Components/SceneComponent.h"
#include "MessageLog.h"
#include "PhysicsEngine/PhysicsSettings.h"

#define LOCTEXT_NAMESPACE "MovementComponent"
DEFINE_LOG_CATEGORY_STATIC(LogMovement, Log, All);

//----------------------------------------------------------------------//
// UMovementComponent
//----------------------------------------------------------------------//
UMovementComponent::UMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;

	MoveComponentFlags = MOVECOMP_NoFlags;

	bUpdateOnlyIfRendered = false;
	bAutoUpdateTickRegistration = true;
	bAutoRegisterUpdatedComponent = true;

	PlaneConstraintNormal = FVector::ZeroVector;
	PlaneConstraintAxisSetting = EPlaneConstraintAxisSetting::Custom;
	bConstrainToPlane = false;
	bSnapToPlaneAtStart = false;

	bWantsInitializeComponent = true;
	bAutoActivate = true;
	bInOnRegister = false;
	bInInitializeComponent = false;
}


void UMovementComponent::SetUpdatedComponent(UPrimitiveComponent* NewUpdatedComponent)
{
	if (UpdatedComponent && UpdatedComponent != NewUpdatedComponent)
	{
		UpdatedComponent->bShouldUpdatePhysicsVolume = false;
		if (!UpdatedComponent->IsPendingKill())
		{
			UpdatedComponent->SetPhysicsVolume(NULL, true);
			UpdatedComponent->PhysicsVolumeChangedDelegate.RemoveDynamic(this, &UMovementComponent::PhysicsVolumeChanged);
		}

		// remove from tick prerequisite
		UpdatedComponent->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick); 
	}

	// Don't assign pending kill components, but allow those to null out previous UpdatedComponent.
	UpdatedComponent = IsValid(NewUpdatedComponent) ? NewUpdatedComponent : NULL;

	if (IsValid(UpdatedComponent))
	{
		UpdatedComponent->bShouldUpdatePhysicsVolume = true;
		UpdatedComponent->PhysicsVolumeChangedDelegate.AddUniqueDynamic(this, &UMovementComponent::PhysicsVolumeChanged);

		if (!bInOnRegister && !bInInitializeComponent)
		{
			// UpdateOverlaps() in component registration will take care of this.
			UpdatedComponent->UpdatePhysicsVolume(true);
		}
		
		// force ticks after movement component updates
		UpdatedComponent->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick); 
	}

	UpdateTickRegistration();

	if (bSnapToPlaneAtStart)
	{
		SnapUpdatedComponentToPlane();
	}
}


void UMovementComponent::InitializeComponent()
{
	TGuardValue<bool> InInitializeComponentGuard(bInInitializeComponent, true);
	Super::InitializeComponent();

	UPrimitiveComponent* NewUpdatedComponent = NULL;
	if (UpdatedComponent != NULL)
	{
		NewUpdatedComponent = UpdatedComponent;
	}
	else if (bAutoRegisterUpdatedComponent)
	{
		// Auto-register owner's root primitive component if found.
		AActor* MyActor = GetOwner();
		if (MyActor)
		{
			NewUpdatedComponent = Cast<UPrimitiveComponent>(MyActor->GetRootComponent());
			if (!NewUpdatedComponent)
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("NoRootPrimitiveWarning", "Movement component {0} must update a PrimitiveComponent, but owning actor '{1}' does not have a root PrimitiveComponent. Auto registration failed."),
					FText::FromString(GetName()),
					FText::FromString(MyActor->GetName())
					));
			}
		}
	}

	SetUpdatedComponent(NewUpdatedComponent);
}


void UMovementComponent::OnRegister()
{
	TGuardValue<bool> InOnRegisterGuard(bInOnRegister, true);
	Super::OnRegister();

	if (PlaneConstraintAxisSetting != EPlaneConstraintAxisSetting::Custom)
	{
		SetPlaneConstraintAxisSetting(PlaneConstraintAxisSetting);
	}

	PlaneConstraintNormal = PlaneConstraintNormal.GetSafeNormal();

	if (bSnapToPlaneAtStart)
	{
		SnapUpdatedComponentToPlane();
	}
}

void UMovementComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	// Super may start up the tick function when we don't want to.
	UpdateTickRegistration();

	// If the owner ticks, make sure we tick first
	AActor* Owner = GetOwner();
	if (bRegister && PrimaryComponentTick.bCanEverTick && Owner && Owner->CanEverTick())
	{
		Owner->PrimaryActorTick.AddPrerequisite(this, PrimaryComponentTick);
	}
}

void UMovementComponent::UpdateTickRegistration()
{
	if (bAutoUpdateTickRegistration)
	{
		const bool bHasUpdatedComponent = (UpdatedComponent != NULL);
		SetComponentTickEnabled(bHasUpdatedComponent && bAutoActivate);
	}
}


void UMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Don't hang on to stale references to a destroyed UpdatedComponent.
	if (UpdatedComponent != NULL && UpdatedComponent->IsPendingKill())
	{
		SetUpdatedComponent(NULL);
	}
}


void UMovementComponent::PostLoad()
{
	Super::PostLoad();

	if (PlaneConstraintAxisSetting == EPlaneConstraintAxisSetting::UseGlobalPhysicsSetting)
	{
		// Make sure to use the most up-to-date project setting in case it has changed.
		PlaneConstraintNormal = GetPlaneConstraintNormalFromAxisSetting(PlaneConstraintAxisSetting);
	}
}


#if WITH_EDITOR
void UMovementComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UMovementComponent, PlaneConstraintAxisSetting))
		{
			PlaneConstraintNormal = GetPlaneConstraintNormalFromAxisSetting(PlaneConstraintAxisSetting);
		}
		else if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UMovementComponent, PlaneConstraintNormal))
		{
			PlaneConstraintAxisSetting = EPlaneConstraintAxisSetting::Custom;
		}
	}
}

void UMovementComponent::PhysicsLockedAxisSettingChanged()
{
	
	for (TObjectIterator<UMovementComponent> Iter; Iter; ++Iter)
	{
		UMovementComponent* MovementComponent = *Iter;
		if (MovementComponent->PlaneConstraintAxisSetting == EPlaneConstraintAxisSetting::UseGlobalPhysicsSetting)
		{
			MovementComponent->PlaneConstraintNormal = MovementComponent->GetPlaneConstraintNormalFromAxisSetting(MovementComponent->PlaneConstraintAxisSetting);
		}
	}
}

#endif // WITH_EDITOR


void UMovementComponent::PhysicsVolumeChanged(APhysicsVolume* NewVolume)
{
	//UE_LOG(LogMovement, Log, TEXT("PhysicsVolumeChanged : No implementation"));
}

APhysicsVolume* UMovementComponent::GetPhysicsVolume() const
{
	if (UpdatedComponent)
	{
		return UpdatedComponent->GetPhysicsVolume();
	}
	
	return GetWorld()->GetDefaultPhysicsVolume();
}

bool UMovementComponent::IsInWater() const
{
	return GetPhysicsVolume() && GetPhysicsVolume()->bWaterVolume;
}

bool UMovementComponent::ShouldSkipUpdate(float DeltaTime) const
{
	if (UpdatedComponent == NULL || UpdatedComponent->Mobility != EComponentMobility::Movable)
	{
		return true;
	}

	if (bUpdateOnlyIfRendered)
	{
		if (GetNetMode() == NM_DedicatedServer)
		{
			// Dedicated servers never render
			return true;
		}

		const float RenderTimeThreshold = 0.41f;
		UWorld* TheWorld = GetWorld();
		if (TheWorld->TimeSince(UpdatedComponent->LastRenderTime) <= RenderTimeThreshold)
		{
			return false; // Rendered, don't skip it.
		}

		// Most components used with movement components don't actually render, so check attached children render times.
		TArray<USceneComponent*> Children;
		UpdatedComponent->GetChildrenComponents(true, Children);
		for (auto Child : Children)
		{
			const UPrimitiveComponent* PrimitiveChild = Cast<UPrimitiveComponent>(Child);
			if (PrimitiveChild)
			{
				if (PrimitiveChild->IsRegistered() && TheWorld->TimeSince(PrimitiveChild->LastRenderTime) <= RenderTimeThreshold)
				{
					return false; // Rendered, don't skip it.
				}
			}
		}

		// No children were recently rendered, safely skip the update.
		return true;
	}

	return false;
}


float UMovementComponent::GetGravityZ() const
{
	return GetPhysicsVolume()->GetGravityZ();
}

void UMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	
}

void UMovementComponent::UpdateComponentVelocity()
{
	if ( UpdatedComponent )
	{
		UpdatedComponent->ComponentVelocity = Velocity;
	}
}

void UMovementComponent::InitCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const
{
	if (UpdatedComponent)
	{
		UpdatedComponent->InitSweepCollisionParams(OutParams, OutResponseParam);
	}
}

bool UMovementComponent::OverlapTest(const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, const FCollisionShape& CollisionShape, const AActor* IgnoreActor) const
{
	static FName NAME_TestOverlap = FName(TEXT("MovementOverlapTest"));
	FCollisionQueryParams QueryParams(NAME_TestOverlap, false, IgnoreActor);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(QueryParams, ResponseParam);
	return GetWorld()->OverlapTest(Location, RotationQuat, CollisionChannel, CollisionShape, QueryParams, ResponseParam);
}

bool UMovementComponent::IsExceedingMaxSpeed(float MaxSpeed) const
{
	MaxSpeed = FMath::Max(0.f, MaxSpeed);
	const float MaxSpeedSquared = FMath::Square(MaxSpeed);
	
	// Allow 1% error tolerance, to account for numeric imprecision.
	const float OverVelocityPercent = 1.01f;
	return (Velocity.SizeSquared() > MaxSpeedSquared * OverVelocityPercent);
}


FVector UMovementComponent::GetPlaneConstraintNormalFromAxisSetting(EPlaneConstraintAxisSetting AxisSetting) const
{
	if (AxisSetting == EPlaneConstraintAxisSetting::UseGlobalPhysicsSetting)
	{
		ESettingsLockedAxis::Type GlobalSetting = UPhysicsSettings::Get()->LockedAxis;
		switch (GlobalSetting)
		{
		case ESettingsLockedAxis::None:	return FVector::ZeroVector;
		case ESettingsLockedAxis::X:	return FVector(1.f, 0.f, 0.f);
		case ESettingsLockedAxis::Y:	return FVector(0.f, 1.f, 0.f);
		case ESettingsLockedAxis::Z:	return FVector(0.f, 0.f, 1.f);
		default:
			checkf(false, TEXT("GetPlaneConstraintNormalFromAxisSetting: Unknown global axis setting %d for %s"), int32(GlobalSetting), *GetNameSafe(GetOwner()));
			return FVector::ZeroVector;
		}
	}

	switch(AxisSetting)
	{
	case EPlaneConstraintAxisSetting::Custom:	return PlaneConstraintNormal;
	case EPlaneConstraintAxisSetting::X:		return FVector(1.f, 0.f, 0.f);
	case EPlaneConstraintAxisSetting::Y:		return FVector(0.f, 1.f, 0.f);
	case EPlaneConstraintAxisSetting::Z:		return FVector(0.f, 0.f, 1.f);
	default:
		checkf(false, TEXT("GetPlaneConstraintNormalFromAxisSetting: Unknown axis %d for %s"), int32(AxisSetting), *GetNameSafe(GetOwner()));
		return FVector::ZeroVector;
	}
}

void UMovementComponent::SetPlaneConstraintAxisSetting(EPlaneConstraintAxisSetting NewAxisSetting)
{
	PlaneConstraintAxisSetting = NewAxisSetting;
	PlaneConstraintNormal = GetPlaneConstraintNormalFromAxisSetting(PlaneConstraintAxisSetting);
}

void UMovementComponent::SetPlaneConstraintNormal(FVector PlaneNormal)
{
	PlaneConstraintNormal = PlaneNormal.GetSafeNormal();
	PlaneConstraintAxisSetting = EPlaneConstraintAxisSetting::Custom;
}

void UMovementComponent::SetPlaneConstraintFromVectors(FVector Forward, FVector Up)
{
	PlaneConstraintNormal = (Up ^ Forward).GetSafeNormal();
}

void UMovementComponent::SetPlaneConstraintOrigin(FVector PlaneOrigin)
{
	PlaneConstraintOrigin = PlaneOrigin;
}

const FVector& UMovementComponent::GetPlaneConstraintOrigin() const
{
	return PlaneConstraintOrigin;
}

const FVector& UMovementComponent::GetPlaneConstraintNormal() const
{
	return PlaneConstraintNormal;
}

FVector UMovementComponent::ConstrainDirectionToPlane(FVector Direction) const
{
	if (bConstrainToPlane)
	{
		Direction = FVector::VectorPlaneProject(Direction, PlaneConstraintNormal);
	}

	return Direction;
}


FVector UMovementComponent::ConstrainLocationToPlane(FVector Location) const
{
	if (bConstrainToPlane)
	{
		Location = FVector::PointPlaneProject(Location, PlaneConstraintOrigin, PlaneConstraintNormal);
	}

	return Location;
}


FVector UMovementComponent::ConstrainNormalToPlane(FVector Normal) const
{
	if (bConstrainToPlane)
	{
		Normal = FVector::VectorPlaneProject(Normal, PlaneConstraintNormal).GetSafeNormal();
	}

	return Normal;
}


void UMovementComponent::SnapUpdatedComponentToPlane()
{
	if (UpdatedComponent && bConstrainToPlane)
	{
		UpdatedComponent->SetWorldLocation( ConstrainLocationToPlane(UpdatedComponent->GetComponentLocation()) );
	}
}



bool UMovementComponent::MoveUpdatedComponent( const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit)
{
	if (UpdatedComponent)
	{
		const FVector NewDelta = ConstrainDirectionToPlane(Delta);
		return UpdatedComponent->MoveComponent(NewDelta, NewRotation, bSweep, OutHit, MoveComponentFlags);
	}

	return false;
}


bool UMovementComponent::K2_MoveUpdatedComponent(FVector Delta, FRotator NewRotation, FHitResult& OutHit, bool bSweep)
{
	return SafeMoveUpdatedComponent(Delta, NewRotation, bSweep, OutHit);
}


bool UMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult& OutHit)
{
	if (UpdatedComponent == NULL)
	{
		OutHit.Reset(1.f);
		return false;
	}

	bool bMoveResult = MoveUpdatedComponent(Delta, NewRotation, bSweep, &OutHit);

	// Handle initial penetrations
	if (OutHit.bStartPenetrating && UpdatedComponent)
	{
		const FVector RequestedAdjustment = GetPenetrationAdjustment(OutHit);
		if (ResolvePenetration(RequestedAdjustment, OutHit, NewRotation))
		{
			// Retry original move
			bMoveResult = MoveUpdatedComponent(Delta, NewRotation, bSweep, &OutHit);
		}
	}

	return bMoveResult;
}

static TAutoConsoleVariable<float> CVarPenetrationPullbackDistance(TEXT("p.PenetrationPullbackDistance"),
	0.125f,
	TEXT("Pull out from penetration of an object by this extra distance.\n")
	TEXT("Distance added to penetration fix-ups."),
	ECVF_Default);

FVector UMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
{
	if (!Hit.bStartPenetrating)
	{
		return FVector::ZeroVector;
	}

	FVector Result;
	const float PullBackDistance = FMath::Abs(CVarPenetrationPullbackDistance.GetValueOnGameThread());
	const float PenetrationDepth = (Hit.PenetrationDepth > 0.f ? Hit.PenetrationDepth : 0.125f);

	Result = Hit.Normal * (PenetrationDepth + PullBackDistance);

	return ConstrainDirectionToPlane(Result);
}

bool UMovementComponent::ResolvePenetration(const FVector& ProposedAdjustment, const FHitResult& Hit, const FRotator& NewRotation)
{
	const FVector Adjustment = ConstrainDirectionToPlane(ProposedAdjustment);
	if (!Adjustment.IsZero() && UpdatedComponent)
	{
		// See if we can fit at the adjusted location without overlapping anything.
		AActor* ActorOwner = UpdatedComponent->GetOwner();
		if (!ActorOwner)
		{
			return false;
		}

		// We really want to make sure that precision differences or differences between the overlap test and sweep tests don't put us into another overlap,
		// so make the overlap test a bit more restrictive.
		const float OverlapInflation = 0.10f;
		bool bEncroached = OverlapTest(Hit.TraceStart + Adjustment, NewRotation.Quaternion(), UpdatedComponent->GetCollisionObjectType(), UpdatedComponent->GetCollisionShape(OverlapInflation), ActorOwner);
		if (!bEncroached)
		{
			// Move without sweeping.
			MoveUpdatedComponent(Adjustment, NewRotation, false);
			UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration: teleport %s by %s"), *ActorOwner->GetName(), *Adjustment.ToString());
			return true;
		}
		else
		{
			// Disable MOVECOMP_NeverIgnoreBlockingOverlaps if it is enabled, otherwise we wouldn't be able to sweep out of the object to fix the penetration.
			TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, EMoveComponentFlags(MoveComponentFlags & (~MOVECOMP_NeverIgnoreBlockingOverlaps)));

			// Try sweeping as far as possible...
			FHitResult SweepOutHit(1.f);
			bool bMoved = MoveUpdatedComponent(Adjustment, NewRotation, true, &SweepOutHit);
			UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration: sweep %s by %s (success = %d)"), *ActorOwner->GetName(), *Adjustment.ToString(), bMoved);
			
			// Still stuck?
			if (!bMoved && SweepOutHit.bStartPenetrating)
			{
				// Combine two MTD results to get a new direction that gets out of multiple surfaces.
				const FVector SecondMTD = GetPenetrationAdjustment(SweepOutHit);
				const FVector CombinedMTD = Adjustment + SecondMTD;
				if (SecondMTD != Adjustment && !CombinedMTD.IsZero())
				{
					bMoved = MoveUpdatedComponent(CombinedMTD, NewRotation, true);
					UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration: sweep %s by %s (MTD combo success = %d)"), *ActorOwner->GetName(), *CombinedMTD.ToString(), bMoved);
				}
			}

			// Still stuck?
			if (!bMoved)
			{
				// Try moving the proposed adjustment plus the attempted move direction. This can sometimes get out of penetrations with multiple objects
				const FVector MoveDelta = ConstrainDirectionToPlane(Hit.TraceEnd - Hit.TraceStart);
				if (!MoveDelta.IsZero())
				{
					bMoved = MoveUpdatedComponent(Adjustment + MoveDelta, NewRotation, true);
					UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration: sweep %s by %s (adjusted attempt success = %d)"), *ActorOwner->GetName(), *(Adjustment + MoveDelta).ToString(), bMoved);
				}
			}	

			return bMoved;
		}
	}

	return false;
}


FVector UMovementComponent::ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const
{
	if (!bConstrainToPlane)
	{
		return FVector::VectorPlaneProject(Delta, Normal) * Time;
	}
	else
	{
		const FVector ProjectedNormal = ConstrainNormalToPlane(Normal);
		return FVector::VectorPlaneProject(Delta, ProjectedNormal) * Time;
	}
}


float UMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PercentTimeApplied = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = ComputeSlideVector(Delta, Time, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		const FRotator Rotation = UpdatedComponent->GetComponentRotation();
		SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit);

		const float FirstHitPercent = Hit.Time;
		PercentTimeApplied = FirstHitPercent;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (bHandleImpact)
			{
				HandleImpact(Hit, FirstHitPercent * Time, SlideDelta);
			}

			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(SlideDelta, Hit, OldHitNormal);

			// Only proceed if the new direction is of significant length.
			if (!SlideDelta.IsNearlyZero(1e-3f))
			{
				// Perform second move
				SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit);
				const float SecondHitPercent = Hit.Time * (1.f - FirstHitPercent);
				PercentTimeApplied += SecondHitPercent;

				// Notify second impact
				if (bHandleImpact && Hit.bBlockingHit)
				{
					HandleImpact(Hit, SecondHitPercent * Time, SlideDelta);
				}
			}
		}

		return FMath::Clamp(PercentTimeApplied, 0.f, 1.f);
	}

	return 0.f;
}


void UMovementComponent::TwoWallAdjust(FVector& OutDelta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	FVector Delta = OutDelta;
	const FVector HitNormal = Hit.Normal;

	if ((OldHitNormal | HitNormal) <= 0.f) //90 or less corner, so use cross product for direction
	{
		const FVector DesiredDir = Delta;
		FVector NewDir = (HitNormal ^ OldHitNormal);
		NewDir = NewDir.GetSafeNormal();
		Delta = (Delta | NewDir) * (1.f - Hit.Time) * NewDir;
		if ((DesiredDir | Delta) < 0.f)
		{
			Delta = -1.f * Delta;
		}
	}
	else //adjust to new wall
	{
		const FVector DesiredDir = Delta;
		Delta = ComputeSlideVector(Delta, 1.f - Hit.Time, HitNormal, Hit);
		if ((Delta | DesiredDir) <= 0.f)
		{
			Delta = FVector::ZeroVector;
		}
		else if ( FMath::Abs((HitNormal | OldHitNormal) - 1.f) < KINDA_SMALL_NUMBER )
		{
			// we hit the same wall again even after adjusting to move along it the first time
			// nudge away from it (this can happen due to precision issues)
			Delta += HitNormal * 0.01f;
		}
	}

	OutDelta = Delta;
}

void UMovementComponent::AddRadialForce(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff)
{
	// Default implementation does nothing
}

void UMovementComponent::AddRadialImpulse(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bVelChange)
{
	// Default implementation does nothing
}

// TODO: Deprecated, remove.
float UMovementComponent::GetMaxSpeedModifier() const
{
	return 1.0f;
}

// TODO: Deprecated, remove.
float UMovementComponent::K2_GetMaxSpeedModifier() const
{
	// Allow calling old deprecated function to maintain old behavior until it is removed.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GetMaxSpeedModifier();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

// TODO: Deprecated, remove.
float UMovementComponent::GetModifiedMaxSpeed() const
{
	return GetMaxSpeed();
}

// TODO: Deprecated, remove.
float UMovementComponent::K2_GetModifiedMaxSpeed() const
{
	// Allow calling old deprecated function to maintain old behavior until it is removed.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GetModifiedMaxSpeed();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#undef LOCTEXT_NAMESPACE
