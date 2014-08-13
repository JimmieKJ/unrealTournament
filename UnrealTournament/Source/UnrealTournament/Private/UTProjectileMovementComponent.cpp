// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTRemoteRedeemer.h"
#include "Net/UnrealNetwork.h"
#include "UTProjectileMovementComponent.h"

UUTProjectileMovementComponent::UUTProjectileMovementComponent(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HitZStopSimulatingThreshold = -1.1f; // default is always stop
}

void UUTProjectileMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (bAutoRegisterUpdatedComponent && AddlUpdatedComponents.Num() == 0 && UpdatedComponent != NULL)
	{
		TArray<USceneComponent*> Components;
		UpdatedComponent->GetChildrenComponents(true, Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Components[i]);
			if (Prim != NULL && Prim->GetCollisionEnabled())
			{
				AddlUpdatedComponents.Add(Prim);
				// if code hasn't manually set an overlap event, mirror the setting of the parent
				if (!Prim->OnComponentBeginOverlap.IsBound() && !Prim->OnComponentEndOverlap.IsBound())
				{
					Prim->OnComponentBeginOverlap = UpdatedComponent->OnComponentBeginOverlap;
					Prim->OnComponentEndOverlap = UpdatedComponent->OnComponentEndOverlap;
				}
			}
		}
	}
}

bool UUTProjectileMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit)
{
	// if we have no extra components or we don't need to sweep, use the default behavior
	if (AddlUpdatedComponents.Num() == 0 || UpdatedComponent == NULL || !bSweep || Delta.IsNearlyZero())
	{
		return Super::MoveUpdatedComponent(Delta, NewRotation, bSweep, OutHit);
	}
	else
	{
		// make sure elements are valid
		AddlUpdatedComponents.Remove(NULL);

		struct FNewableScopedMovementUpdate : public FScopedMovementUpdate
		{
		public:
			// relative transform of component prior to move
			FVector RelativeLocation;
			FRotator RelativeRotation;

			FNewableScopedMovementUpdate(USceneComponent* Component, EScopedUpdate::Type ScopeBehavior = EScopedUpdate::DeferredUpdates)
				: FScopedMovementUpdate(Component, ScopeBehavior), RelativeLocation(Component->RelativeLocation), RelativeRotation(Component->RelativeRotation)
			{}

			// allow new and delete on FScopedMovementUpdate so we can use a dynamic number of elements
			// WARNING: these MUST all be deleted before this function returns in opposite order of creation or bad stuff will happen!
			REPLACEMENT_OPERATOR_NEW_AND_DELETE
		};

		FNewableScopedMovementUpdate* RootDeferredUpdate = new FNewableScopedMovementUpdate(UpdatedComponent);
		TArray<FNewableScopedMovementUpdate*> DeferredUpdates;
		for (int32 i = 0; i < AddlUpdatedComponents.Num(); i++)
		{
			DeferredUpdates.Add(new FNewableScopedMovementUpdate(AddlUpdatedComponents[i]));
		}

		FRotator RotChange = NewRotation.GetNormalized() - UpdatedComponent->ComponentToWorld.GetRotation().Rotator().GetNormalized();

		FHitResult EarliestHit;
		// move root
		bool bResult = Super::MoveUpdatedComponent(Delta, NewRotation, bSweep, &EarliestHit);
		float InitialMoveSize = Delta.Size() * EarliestHit.Time;
		// move children
		float ShortestMoveSize = InitialMoveSize;
		bool bGotHit = EarliestHit.bBlockingHit;
		for (int32 i = 0; i < AddlUpdatedComponents.Num(); i++)
		{
			// hack so InternalSetWorldLocationAndRotation() counts the component as moved (which will also clobber this hacked value)
			// otherwise it will pull the updated transform from the parent above and think nothing has happened, which prevents overlaps from working
			AddlUpdatedComponents[i]->RelativeLocation.Z += 0.1f;

			FHitResult NewHit;
			AddlUpdatedComponents[i]->MoveComponent(Delta, AddlUpdatedComponents[i]->ComponentToWorld.GetRotation().Rotator() + RotChange, bSweep, &NewHit, MoveComponentFlags);
			if (NewHit.bBlockingHit)
			{
				float MoveSize = Delta.Size() * NewHit.Time;
				if (!bGotHit || MoveSize < ShortestMoveSize - KINDA_SMALL_NUMBER)
				{
					ShortestMoveSize = MoveSize;
					EarliestHit = NewHit;
				}
				bGotHit = true;
			}
			// restore RelativeLocation and RelativeRotation after moving
			AddlUpdatedComponents[i]->SetRelativeLocationAndRotation(DeferredUpdates[i]->RelativeLocation, DeferredUpdates[i]->RelativeRotation, false);
		}
		// if we got a blocking hit, we need to revert and move everything using the shortest delta
		if (bGotHit)
		{
			static bool bRecursing = false; // recursion should be impossible but sanity check
			if (bRecursing)
			{
				// apply moves
				for (int32 i = DeferredUpdates.Num() - 1; i >= 0; i--)
				{
					delete DeferredUpdates[i];
				}
				delete RootDeferredUpdate;

				if (OutHit != NULL)
				{
					(*OutHit) = EarliestHit;
				}
				return bResult;
			}
			else
			{
				bRecursing = true;

				// revert moves
				for (int32 i = DeferredUpdates.Num() - 1; i >= 0; i--)
				{
					DeferredUpdates[i]->RevertMove();
					delete DeferredUpdates[i];
				}
				RootDeferredUpdate->RevertMove();
				delete RootDeferredUpdate;
				// recurse
				bResult = MoveUpdatedComponent(Delta.SafeNormal() * ShortestMoveSize, NewRotation, bSweep, OutHit);
				if (OutHit != NULL)
				{
					(*OutHit) = EarliestHit;
				}
				return bResult;
			}
		}
		else
		{
			// apply moves
			for (int32 i = DeferredUpdates.Num() - 1; i >= 0; i--)
			{
				delete DeferredUpdates[i];
			}
			delete RootDeferredUpdate;

			if (OutHit != NULL)
			{
				(*OutHit) = EarliestHit;
			}
			return bResult;
		}
	}
}

// this is a hack around UProjectileMovementComponent not passing the real TimeSlice to HandleImpact()
static float LastTimeSlice = 0.0f;
FVector UUTProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime, bool bApplyGravity) const
{
	LastTimeSlice = DeltaTime;
	return Super::ComputeMoveDelta(InVelocity, DeltaTime, bApplyGravity);
}
void UUTProjectileMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	LastTimeSlice = 0.0f;
	UpdateState(DeltaTime);
}
void UUTProjectileMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	if (!bShouldBounce && Hit.Normal.Z < HitZStopSimulatingThreshold && UpdatedComponent != NULL && UpdatedComponent->GetOwner() != NULL && (TimeSlice > 0.0f || LastTimeSlice > 0.0f))
	{
		AActor* ActorOwner = UpdatedComponent->GetOwner();
		FVector OldLocation = UpdatedComponent->GetComponentLocation();

		// restore time and velocity that got clobbered by UProjectileMovementComponent::TickComponent()'s hit handling
		if (TimeSlice == 0.0f)
		{
			TimeSlice = LastTimeSlice;
		}
		else
		{
			TimeSlice /= Hit.Time;
		}
		Velocity = MoveDelta / TimeSlice;

		FVector OldHitNormal = Hit.Normal;
		FVector Delta = (MoveDelta - Hit.Normal * (MoveDelta | Hit.Normal)) * (1.f - Hit.Time);
		if ((Delta | MoveDelta) >= 0.0f)
		{
			if (Delta.Z > 0.f) // friction slows sliding up slopes
			{
				Delta *= 0.5f;
			}
			FHitResult NewHit;
			SafeMoveUpdatedComponent(Delta, ActorOwner->GetActorRotation(), true, NewHit);
			if (UpdatedComponent != NULL && !ActorOwner->bPendingKillPending)
			{
				if (NewHit.Time < 1.f) // hit second wall
				{
					if (NewHit.Normal.Z >= HitZStopSimulatingThreshold)
					{
						StopSimulating(NewHit);
					}
					else
					{
						// TODO: should we call anything here?
						//processHitWall(NewHit.Normal, NewHit.Actor, NewHit.Component);
						//if (bDeleteMe)
						//{
						//	return;
						//}

						TwoWallAdjust(Delta, NewHit, OldHitNormal);
						if (UpdatedComponent != NULL && !ActorOwner->bPendingKillPending)
						{
							bool bDitch = ((OldHitNormal.Z > 0.0f) && (NewHit.Normal.Z > 0.0f) && (Delta.Z == 0.0f) && ((NewHit.Normal | OldHitNormal) < 0.0f));
							SafeMoveUpdatedComponent(Delta, ActorOwner->GetActorRotation(), true, NewHit);
							if (UpdatedComponent != NULL && !ActorOwner->bPendingKillPending)
							{
								if (bDitch || NewHit.Normal.Z >= HitZStopSimulatingThreshold)
								{
									StopSimulating(NewHit);
								}
							}
						}
					}
				}
			}
		}
		// update velocity for actual movement that occurred
		if (UpdatedComponent != NULL)
		{
			float OldVelZ = Velocity.Z;
			Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation + MoveDelta * Hit.Time) / TimeSlice;
			Velocity.Z = OldVelZ;
		}
	}
	else
	{
		Super::HandleImpact(Hit, TimeSlice, MoveDelta);
	}
}

void UUTProjectileMovementComponent::UpdateState(float DeltaSeconds)
{	
	AUTRemoteRedeemer* MyRedeemer = UpdatedComponent ? Cast<AUTRemoteRedeemer>(UpdatedComponent->GetOwner()) : NULL;
	if (MyRedeemer)
	{
		if (MyRedeemer->IsLocallyControlled())
		{
			// Can only start sending moves if our controllers are synced up over the network, otherwise we flood the reliable buffer.
			APlayerController* PC = Cast<APlayerController>(MyRedeemer->GetController());
			if (PC && PC->AcknowledgedPawn != MyRedeemer)
			{
				return;
			}

			ServerUpdateState(Acceleration);
		}
		else
		{
			Acceleration = ReplicatedAcceleration;
		}
	}
}

void UUTProjectileMovementComponent::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UUTProjectileMovementComponent, ReplicatedAcceleration);
}

bool UUTProjectileMovementComponent::ServerUpdateState_Validate(FVector InAcceleration)
{
	return true;
}

void UUTProjectileMovementComponent::ServerUpdateState_Implementation(FVector InAcceleration)
{
	bool bServerReadyForClient = true;
	AUTRemoteRedeemer* MyRedeemer = UpdatedComponent ? Cast<AUTRemoteRedeemer>(UpdatedComponent->GetOwner()) : NULL;
	if (MyRedeemer)
	{
		APlayerController* PC = Cast<APlayerController>(MyRedeemer->GetController());
		if (PC)
		{
			bServerReadyForClient = PC->NotifyServerReceivedClientData(MyRedeemer, 0.0f);
		}
	}

	if (!bServerReadyForClient)
	{
		return;
	}

	Acceleration = InAcceleration;
	ReplicatedAcceleration = Acceleration;
}