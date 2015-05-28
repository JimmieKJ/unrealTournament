// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/LineBatchComponent.h"
#include "MessageLog.h"

//////////////// PRIMITIVECOMPONENT ///////////////

#define LOCTEXT_NAMESPACE "PrimitiveComponent"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define WarnInvalidPhysicsOperations(Text, BodyInstance)  { static const FText _WarnText(Text); WarnInvalidPhysicsOperations_Internal(_WarnText, BodyInstance); }
#else
	#define WarnInvalidPhysicsOperations(Text, BodyInstance)
#endif


bool UPrimitiveComponent::ApplyRigidBodyState(const FRigidBodyState& NewState, const FRigidBodyErrorCorrection& ErrorCorrection, FVector& OutDeltaPos, FName BoneName)
{
	bool bRestoredState = true;

	FBodyInstance* BI = GetBodyInstance(BoneName);
	if (BI && BI->IsInstanceSimulatingPhysics())
	{
		// failure cases
		const float QuatSizeSqr = NewState.Quaternion.SizeSquared();
		if (QuatSizeSqr < KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogPhysics, Warning, TEXT("Invalid zero quaternion set for body. (%s:%s)"), *GetName(), *BoneName.ToString());
			return bRestoredState;
		}
		else if (FMath::Abs(QuatSizeSqr - 1.f) > KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogPhysics, Warning, TEXT("Quaternion (%f %f %f %f) with non-unit magnitude detected. (%s:%s)"), 
				NewState.Quaternion.X, NewState.Quaternion.Y, NewState.Quaternion.Z, NewState.Quaternion.W, *GetName(), *BoneName.ToString() );
			return bRestoredState;
		}

		FRigidBodyState CurrentState;
		GetRigidBodyState(CurrentState, BoneName);

		const bool bShouldSleep = (NewState.Flags & ERigidBodyFlags::Sleeping) != 0;

		/////// POSITION CORRECTION ///////

		// Find out how much of a correction we are making
		const FVector DeltaPos = NewState.Position - CurrentState.Position;
		const float DeltaMagSq = DeltaPos.SizeSquared();
		const float BodyLinearSpeedSq = CurrentState.LinVel.SizeSquared();

		// Snap position by default (big correction, or we are moving too slowly)
		FVector UpdatedPos = NewState.Position;
		FVector FixLinVel = FVector::ZeroVector;

		// If its a small correction and velocity is above threshold, only make a partial correction, 
		// and calculate a velocity that would fix it over 'fixTime'.
		if (DeltaMagSq < ErrorCorrection.LinearDeltaThresholdSq  &&
			BodyLinearSpeedSq >= ErrorCorrection.BodySpeedThresholdSq)
		{
			UpdatedPos = FMath::Lerp(CurrentState.Position, NewState.Position, ErrorCorrection.LinearInterpAlpha);
			FixLinVel = (NewState.Position - UpdatedPos) * ErrorCorrection.LinearRecipFixTime;
		}

		// Get the linear correction
		OutDeltaPos = UpdatedPos - CurrentState.Position;

		/////// ORIENTATION CORRECTION ///////
		// Get quaternion that takes us from old to new
		const FQuat InvCurrentQuat = CurrentState.Quaternion.Inverse();
		const FQuat DeltaQuat = NewState.Quaternion * InvCurrentQuat;

		FVector DeltaAxis;
		float DeltaAng;	// radians
		DeltaQuat.ToAxisAndAngle(DeltaAxis, DeltaAng);
		DeltaAng = FMath::UnwindRadians(DeltaAng);

		// Snap rotation by default (big correction, or we are moving too slowly)
		FQuat UpdatedQuat = NewState.Quaternion;
		FVector FixAngVel = FVector::ZeroVector; // degrees per second
		
		// If the error is small, and we are moving, try to move smoothly to it
		if (FMath::Abs(DeltaAng) < ErrorCorrection.AngularDeltaThreshold )
		{
			UpdatedQuat = FMath::Lerp(CurrentState.Quaternion, NewState.Quaternion, ErrorCorrection.AngularInterpAlpha);
			FixAngVel = DeltaAxis.GetSafeNormal() * FMath::RadiansToDegrees(DeltaAng) * (1.f - ErrorCorrection.AngularInterpAlpha) * ErrorCorrection.AngularRecipFixTime;
		}

		/////// BODY UPDATE ///////
		BI->SetBodyTransform(FTransform(UpdatedQuat, UpdatedPos), true);
		BI->SetLinearVelocity(NewState.LinVel + FixLinVel, false);
		BI->SetAngularVelocity(NewState.AngVel + FixAngVel, false);

		// state is restored when no velocity corrections are required
		bRestoredState = (FixLinVel.SizeSquared() < KINDA_SMALL_NUMBER) && (FixAngVel.SizeSquared() < KINDA_SMALL_NUMBER);
	
		/////// SLEEP UPDATE ///////
		const bool bIsAwake = BI->IsInstanceAwake();
		if (bIsAwake && (bShouldSleep && bRestoredState))
		{
			BI->PutInstanceToSleep();
		}
		else if (!bIsAwake)
		{
			BI->WakeInstance();
		}
	}

	return bRestoredState;
}

bool UPrimitiveComponent::ConditionalApplyRigidBodyState(FRigidBodyState& UpdatedState, const FRigidBodyErrorCorrection& ErrorCorrection, FVector& OutDeltaPos, FName BoneName)
{
	bool bUpdated = false;

	// force update if simulation is sleeping on server
	if ((UpdatedState.Flags & ERigidBodyFlags::Sleeping) && RigidBodyIsAwake(BoneName))
	{
		UpdatedState.Flags |= ERigidBodyFlags::NeedsUpdate;	
	}

	if (UpdatedState.Flags & ERigidBodyFlags::NeedsUpdate)
	{
		const bool bRestoredState = ApplyRigidBodyState(UpdatedState, ErrorCorrection, OutDeltaPos, BoneName);
		if (bRestoredState)
		{
			UpdatedState.Flags &= ~ERigidBodyFlags::NeedsUpdate;
		}

		bUpdated = true;
	}

	return bUpdated;
}

bool UPrimitiveComponent::GetRigidBodyState(FRigidBodyState& OutState, FName BoneName)
{
	FBodyInstance* BI = GetBodyInstance(BoneName);
	if (BI && BI->IsInstanceSimulatingPhysics())
	{
		FTransform BodyTM = BI->GetUnrealWorldTransform();
		OutState.Position = BodyTM.GetTranslation();
		OutState.Quaternion = BodyTM.GetRotation();
		OutState.LinVel = BI->GetUnrealWorldVelocity();
		OutState.AngVel = BI->GetUnrealWorldAngularVelocity();
		OutState.Flags = (BI->IsInstanceAwake() ? ERigidBodyFlags::None : ERigidBodyFlags::Sleeping);
		return true;
	}

	return false;
}

const struct FWalkableSlopeOverride& UPrimitiveComponent::GetWalkableSlopeOverride() const
{
	return BodyInstance.GetWalkableSlopeOverride();
}

void UPrimitiveComponent::SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride)
{
	BodyInstance.SetWalkableSlopeOverride(NewOverride);
}

void UPrimitiveComponent::WarnInvalidPhysicsOperations_Internal(const FText& ActionText, const FBodyInstance* BI) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!CheckStaticMobilityAndWarn(ActionText))	//all physics operations require non-static mobility
	{
		if (BI && BI->bSimulatePhysics == false)	//some require to be simulating too
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("InvalidPhysicsOperation", "{0} has to have 'Simulate Physics' enabled if you'd like to {1}. "), FText::FromString(GetReadableName()), ActionText));
		}
	}
#endif
}


void UPrimitiveComponent::SetSimulatePhysics(bool bSimulate)
{
	BodyInstance.SetInstanceSimulatePhysics(bSimulate);
}

void UPrimitiveComponent::SetConstraintMode(EDOFMode::Type ConstraintMode)
{
	FBodyInstance * RootBI = GetBodyInstance(NAME_None, false);

	if (RootBI == NULL || IsPendingKill())
	{
		return;
	}

	RootBI->SetDOFLock(ConstraintMode);
}

void UPrimitiveComponent::SetLockedAxis(EDOFMode::Type LockedAxis)
{
	SetConstraintMode(LockedAxis);
}

void UPrimitiveComponent::AddImpulse(FVector Impulse, FName BoneName, bool bVelChange)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("AddImpulse", "AddImpulse"), BI);
		BI->AddImpulse(Impulse, bVelChange);
	}
}

void UPrimitiveComponent::AddAngularImpulse(FVector Impulse, FName BoneName, bool bVelChange)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("AddAngularImpulse", "AddAngularImpulse"), BI);
		BI->AddAngularImpulse(Impulse, bVelChange);
	}
}

void UPrimitiveComponent::AddImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("AddImpulseAtLocation", "AddImpulseAtLocation"), BI);
		BI->AddImpulseAtPosition(Impulse, Location);
	}
}

void UPrimitiveComponent::AddRadialImpulse(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange)
{
	if(bIgnoreRadialImpulse)
	{
		return;
	}

	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		BI->AddRadialImpulseToBody(Origin, Radius, Strength, Falloff, bVelChange);
	}
}


void UPrimitiveComponent::AddForce(FVector Force, FName BoneName, bool bAccelChange)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("AddForce", "AddForce"), BI);
		BI->AddForce(Force, true, bAccelChange);
	}
}

void UPrimitiveComponent::AddForceAtLocation(FVector Force, FVector Location, FName BoneName)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("AddForceAtLocation", "AddForceAtLocation"), BI);
		BI->AddForceAtPosition(Force, Location);
	}
}

void UPrimitiveComponent::AddRadialForce(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bAccelChange)
{
	if(bIgnoreRadialForce)
	{
		return;
	}

	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		BI->AddRadialForceToBody(Origin, Radius, Strength, Falloff, bAccelChange);
	}
}

void UPrimitiveComponent::AddTorque(FVector Torque, FName BoneName, bool bAccelChange)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("AddTorque", "AddTorque"), BI);
		BI->AddTorque(Torque, true, bAccelChange);
	}
}

void UPrimitiveComponent::SetPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent, FName BoneName)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("SetPhysicsLinearVelocity", "SetPhysicsLinearVelocity"), nullptr);
		BI->SetLinearVelocity(NewVel, bAddToCurrent);
	}
}

FVector UPrimitiveComponent::GetPhysicsLinearVelocity(FName BoneName)
{
	FBodyInstance* BI = GetBodyInstance(BoneName);
	if(BI != NULL)
	{
		return BI->GetUnrealWorldVelocity();
	}
	return FVector(0,0,0);
}

void UPrimitiveComponent::SetAllPhysicsLinearVelocity(FVector NewVel,bool bAddToCurrent)
{
	SetPhysicsLinearVelocity(NewVel, bAddToCurrent, NAME_None);
}

void UPrimitiveComponent::SetPhysicsAngularVelocity(FVector NewAngVel, bool bAddToCurrent, FName BoneName)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("SetPhysicsAngularVelocity", "SetPhysicsAngularVelocity"), nullptr);
		BI->SetAngularVelocity(NewAngVel, bAddToCurrent);
	}
}

void UPrimitiveComponent::SetPhysicsMaxAngularVelocity(float NewMaxAngVel, bool bAddToCurrent, FName BoneName)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		WarnInvalidPhysicsOperations(LOCTEXT("SetPhysicsMaxAngularVelocity", "SetPhysicsMaxAngularVelocity"), nullptr);
		BI->SetMaxAngularVelocity(NewMaxAngVel, bAddToCurrent);
	}
}

FVector UPrimitiveComponent::GetPhysicsAngularVelocity(FName BoneName)
{
	FBodyInstance* const BI = GetBodyInstance(BoneName);
	if(BI != NULL)
	{
		return BI->GetUnrealWorldAngularVelocity();
	}
	return FVector(0,0,0);
}

FVector UPrimitiveComponent::GetCenterOfMass(FName BoneName)
{
	if (FBodyInstance* ComponentBodyInstance = GetBodyInstance(BoneName))
	{
		return ComponentBodyInstance->GetCOMPosition();
	}

	return FVector::ZeroVector;
}

void UPrimitiveComponent::SetAllPhysicsAngularVelocity(FVector const& NewAngVel, bool bAddToCurrent)
{
	SetPhysicsAngularVelocity(NewAngVel, bAddToCurrent, NAME_None); 
}


void UPrimitiveComponent::SetAllPhysicsPosition(FVector NewPos)
{
	SetWorldLocation(NewPos, NAME_None);
}


void UPrimitiveComponent::SetAllPhysicsRotation(FRotator NewRot)
{
	SetWorldRotation(NewRot, NAME_None);
}


void UPrimitiveComponent::WakeRigidBody(FName BoneName)
{
	FBodyInstance* BI = GetBodyInstance(BoneName);
	if(BI)
	{
		BI->WakeInstance();
	}
}

void UPrimitiveComponent::WakeAllRigidBodies()
{
	WakeRigidBody(NAME_None);
}

void UPrimitiveComponent::SetEnableGravity(bool bGravityEnabled)
{
	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		BI->SetEnableGravity(bGravityEnabled);
	}
}

bool UPrimitiveComponent::IsGravityEnabled() const
{
	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		return BI->bEnableGravity;
	}

	return false;
}

void UPrimitiveComponent::SetLinearDamping(float InDamping)
{
	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		BI->LinearDamping = InDamping;
		BI->UpdateDampingProperties();
	}
}

float UPrimitiveComponent::GetLinearDamping() const
{

	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		return BI->LinearDamping;
	}
	
	return 0.f;
}

void UPrimitiveComponent::SetAngularDamping(float InDamping)
{
	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		BI->AngularDamping = InDamping;
		BI->UpdateDampingProperties();
	}
}

float UPrimitiveComponent::GetAngularDamping() const
{
	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		return BI->AngularDamping;
	}
	
	return 0.f;
}

void UPrimitiveComponent::SetMassScale(FName BoneName, float InMassScale)
{
	FBodyInstance* BI = GetBodyInstance(BoneName);
	if (BI)
	{
		BI->SetMassScale(InMassScale);
	}
}

void UPrimitiveComponent::SetAllMassScale(float InMassScale)
{
	SetMassScale(NAME_None, InMassScale);
}

float UPrimitiveComponent::GetMass() const
{
	FBodyInstance* BI = GetBodyInstance();
	if (BI)
	{
		return BI->GetBodyMass();
	}

	return 0.0f;
}

FVector UPrimitiveComponent::GetInertiaTensor(FName BoneName /* = NAME_None */) const 
{
	if(FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		return BI->GetBodyInertiaTensor();
	}

	return FVector::ZeroVector;
}

FVector UPrimitiveComponent::ScaleByMomentOfInertia(FVector InputVector, FName BoneName /* = NAME_None */) const
{
	const FVector LocalInertiaTensor = GetInertiaTensor(BoneName);
	const FVector InputVectorLocal = ComponentToWorld.InverseTransformVectorNoScale(InputVector);
	const FVector LocalScaled = InputVectorLocal * LocalInertiaTensor;
	const FVector WorldScaled = ComponentToWorld.TransformVectorNoScale(LocalScaled);
	return WorldScaled;
}

float UPrimitiveComponent::CalculateMass(FName)
{
	if (BodyInstance.bOverrideMass)
	{
		return BodyInstance.MassInKg;
	}

	if (BodyInstance.BodySetup.IsValid())
	{
		return BodyInstance.BodySetup->CalculateMass(this);
	}
	else if (UBodySetup * BodySetup = GetBodySetup())
	{
		return BodySetup->CalculateMass(this);
	}
	return 0.0f;
}

void UPrimitiveComponent::PutRigidBodyToSleep(FName BoneName)
{
	FBodyInstance* BI = GetBodyInstance(BoneName);
	if(BI)
	{
		BI->PutInstanceToSleep();
	}
}

void UPrimitiveComponent::PutAllRigidBodiesToSleep()
{
	PutRigidBodyToSleep(NAME_None);
}


bool UPrimitiveComponent::RigidBodyIsAwake(FName BoneName)
{
	FBodyInstance* BI = GetBodyInstance(BoneName);
	if(BI)
	{
		return BI->IsInstanceAwake();
	}

	return false;
}

bool UPrimitiveComponent::IsAnyRigidBodyAwake()
{
	return RigidBodyIsAwake(NAME_None);
}


void UPrimitiveComponent::SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision)
{
	BodyInstance.SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	OnComponentCollisionSettingsChanged();
}



void UPrimitiveComponent::SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial)
{
	BodyInstance.SetPhysMaterialOverride(NewPhysMaterial);
}

FTransform UPrimitiveComponent::GetComponentTransformFromBodyInstance(FBodyInstance* UseBI)
{
	return UseBI->GetUnrealWorldTransform();
}

void UPrimitiveComponent::SyncComponentToRBPhysics()
{
	if(!IsRegistered())
	{
		UE_LOG(LogPhysics, Log, TEXT("SyncComponentToRBPhysics : Component not registered (%s)"), *GetPathName());
		return;
	}

	 // BodyInstance we are going to sync the component to
	FBodyInstance* UseBI = GetBodyInstance();
	if(UseBI == NULL || !UseBI->IsValidBodyInstance())
	{
		UE_LOG(LogPhysics, Log, TEXT("SyncComponentToRBPhysics : Missing or invalid BodyInstance (%s)"), *GetPathName());
		return;
	}

	AActor* Owner = GetOwner();
	if(Owner != NULL)
	{
		if (Owner->IsPendingKill() || !Owner->CheckStillInWorld())
		{
			return;
		}
	}

	if (IsPendingKill() || !IsSimulatingPhysics())
	{
		return;
	}

	// See if the transform is actually different, and if so, move the component to match physics
	const FTransform NewTransform = GetComponentTransformFromBodyInstance(UseBI);	
	if(!NewTransform.EqualsNoScale(ComponentToWorld))
	{
		const FVector MoveBy = NewTransform.GetLocation() - ComponentToWorld.GetLocation();
		const FQuat NewRotation = NewTransform.GetRotation();

		//@warning: do not reference BodyInstance again after calling MoveComponent() - events from the move could have made it unusable (destroying the actor, SetPhysics(), etc)
		MoveComponent(MoveBy, NewRotation, false, NULL, MOVECOMP_SkipPhysicsMove);
	}
}


UPrimitiveComponent * GetRootWelded(const UPrimitiveComponent* PrimComponent, FName ParentSocketName = NAME_None, FName* OutSocketName = NULL, bool bAboutToWeld = false)
{
	UPrimitiveComponent * Result = NULL;
	UPrimitiveComponent * RootComponent = Cast<UPrimitiveComponent>(PrimComponent->AttachParent);	//we must find the root component along hierarchy that has bWelded set to true

	//check that body itself is welded
	if (FBodyInstance* BI = PrimComponent->GetBodyInstance(ParentSocketName, false))
	{
		if (bAboutToWeld == false && BI->bWelded == false && BI->bAutoWeld == false)	//we're not welded and we aren't trying to become welded
		{
			return NULL;
		}
	}

	FName PrevSocketName = ParentSocketName;
	FName SocketName = NAME_None; //because of skeletal mesh it's important that we check along the bones that we attached
	FBodyInstance* RootBI = NULL;
	for (; RootComponent; RootComponent = Cast<UPrimitiveComponent>(RootComponent->AttachParent))
	{
		Result = RootComponent;
		SocketName = PrevSocketName;
		PrevSocketName = RootComponent->AttachSocketName;

		RootBI = RootComponent->GetBodyInstance(SocketName, false);
		if (RootBI && RootBI->bWelded)
		{
			continue;
		}

		break;
	}

	if (OutSocketName)
	{
		*OutSocketName = SocketName;
	}

	return Result;
}

void UPrimitiveComponent::GetWeldedBodies(TArray<FBodyInstance*> & OutWeldedBodies, TArray<FName> & OutLabels)
{
	OutWeldedBodies.Add(&BodyInstance);
	OutLabels.Add(NAME_None);

	for (USceneComponent * Child : AttachChildren)
	{
		if (UPrimitiveComponent * PrimChild = Cast<UPrimitiveComponent>(Child))
		{
			if (FBodyInstance* BI = PrimChild->GetBodyInstance(NAME_None, false))
			{
				if (BI->bWelded)
				{
					PrimChild->GetWeldedBodies(OutWeldedBodies, OutLabels);
				}
			}
		}
	}
}

bool UPrimitiveComponent::WeldToImplementation(USceneComponent * InParent, FName ParentSocketName /* = Name_None */, bool bWeldSimulatedChild /* = false */)
{
	//WeldToInternal assumes attachment is already done
	if (AttachParent != InParent || AttachSocketName != ParentSocketName)
	{
		return false;
	}

	//Check that we can actually our own socket name
	FBodyInstance* BI = GetBodyInstance(NAME_None, false);
	if (BI == NULL)
	{
		return false;
	}

	if (BI->ShouldInstanceSimulatingPhysics() && bWeldSimulatedChild == false)
	{
		return false;
	}

	UnWeldFromParent();	//make sure to unweld from wherever we currently are

	FName SocketName;
	UPrimitiveComponent * RootComponent = GetRootWelded(this, ParentSocketName, &SocketName, true);

	if (RootComponent)
	{
		if (FBodyInstance* RootBI = RootComponent->GetBodyInstance(SocketName, false))
		{
			if (BI->WeldParent == RootBI)	//already welded so stop
			{
				return true;
			}

			BI->bWelded = true;
			//There are multiple cases to handle:
			//Root is kinematic, simulated
			//Child is kinematic, simulated
			//Child always inherits from root

			//if root is kinematic simply set child to be kinematic and we're done
			if (RootComponent->IsSimulatingPhysics(SocketName) == false)
			{
				BI->WeldParent = NULL;
				SetSimulatePhysics(false);
				return false;	//return false because we need to continue with regular body initialization
			}

			//root is simulated so we actually weld the body
			FTransform RelativeTM = RootComponent == AttachParent ? GetRelativeTransform() : GetComponentToWorld().GetRelativeTransform(RootComponent->GetComponentToWorld());	//if direct parent we already have relative. Otherwise compute it
			RootBI->Weld(BI, GetComponentToWorld());
			BI->WeldParent = RootBI;

			return true;
		}
	}

	return false;
}

void UPrimitiveComponent::WeldTo(USceneComponent* InParent, FName InSocketName /* = NAME_None */)
{
	//automatically attach if needed
	if (AttachParent != InParent || AttachSocketName != InSocketName)
	{
		AttachTo(InParent, InSocketName, EAttachLocation::KeepWorldPosition);
	}

	WeldToImplementation(InParent, InSocketName);
}

void UPrimitiveComponent::UnWeldFromParent()
{
	FBodyInstance* NewRootBI = GetBodyInstance(NAME_None, false);
	UWorld* CurrentWorld = GetWorld();
	if (NewRootBI == NULL || NewRootBI->bWelded == false || CurrentWorld == nullptr || IsPendingKill())
	{
		return;
	}

	FName SocketName;
	UPrimitiveComponent * RootComponent = GetRootWelded(this, AttachSocketName, &SocketName);

	if (RootComponent)
	{
		if (FBodyInstance* RootBI = RootComponent->GetBodyInstance(SocketName, false))
		{
			bool bRootIsBeingDeleted = RootComponent->HasAnyFlags(RF_PendingKill) || RootComponent->HasAnyFlags(RF_Unreachable);
			if (!bRootIsBeingDeleted)
			{
				//create new root
				RootBI->UnWeld(NewRootBI);	//don't bother fixing up shapes if RootComponent is about to be deleted
			}

			NewRootBI->bWelded = false;
			const FBodyInstance* PrevWeldParent = NewRootBI->WeldParent;
			NewRootBI->WeldParent = nullptr;

			bool bHasBodySetup = GetBodySetup() != nullptr;

			//if BodyInstance hasn't already been created we need to initialize it
			if (bHasBodySetup && NewRootBI->IsValidBodyInstance() == false)
			{
				bool bPrevAutoWeld = NewRootBI->bAutoWeld;
				NewRootBI->bAutoWeld = false;
				NewRootBI->InitBody(GetBodySetup(), GetComponentToWorld(), this, CurrentWorld->GetPhysicsScene());
				NewRootBI->bAutoWeld = bPrevAutoWeld;
			}

			if(PrevWeldParent == nullptr)	//our parent is kinematic so no need to do any unwelding/rewelding of children
			{
				return;
			}

			//now weld its children to it
			TArray<FBodyInstance*> ChildrenBodies;
			TArray<FName> ChildrenLabels;
			GetWeldedBodies(ChildrenBodies, ChildrenLabels);

			for (int32 ChildIdx = 0; ChildIdx < ChildrenBodies.Num(); ++ChildIdx)
			{
				FBodyInstance* ChildBI = ChildrenBodies[ChildIdx];
				if (ChildBI != NewRootBI)
				{
					if (!bRootIsBeingDeleted)
					{
						RootBI->UnWeld(ChildBI);
					}

					//At this point, NewRootBI must be kinematic because it's being unwelded. It's up to the code that simulates to call Weld on the children as needed
					ChildBI->WeldParent = nullptr;	//null because we are currently kinematic
				}
			}
		}
	}
}

void UPrimitiveComponent::UnWeldChildren()
{
	for (USceneComponent* ChildComponent : AttachChildren)
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ChildComponent))
		{
			PrimComp->UnWeldFromParent();
		}
	}
}

FBodyInstance* UPrimitiveComponent::GetBodyInstance(FName BoneName, bool bGetWelded) const
{
	if (bGetWelded && BodyInstance.bWelded)
	{
		FName OutSocket;
		if (UPrimitiveComponent * RootComponentWelded = GetRootWelded(this, AttachSocketName, &OutSocket))
		{
			if (FBodyInstance* BI = RootComponentWelded->GetBodyInstance(OutSocket, bGetWelded))
			{
				if (BI->bSimulatePhysics)
				{
					return BI;
				}
			}
		}
	}

	return const_cast<FBodyInstance*>(&BodyInstance);
}

float UPrimitiveComponent::GetDistanceToCollision(const FVector& Point, FVector& ClosestPointOnCollision) const
{
	ClosestPointOnCollision=Point;

	FBodyInstance* BodyInst = GetBodyInstance();
	if(BodyInst != NULL)
	{
		return BodyInst->GetDistanceToBody(Point, ClosestPointOnCollision);
	}

	return -1.f;
}

bool UPrimitiveComponent::IsSimulatingPhysics(FName BoneName) const
{
	FBodyInstance* BodyInst = GetBodyInstance(BoneName);
	if(BodyInst != NULL)
	{
		return BodyInst->IsInstanceSimulatingPhysics();
	}
	else
	{
		return false;
	}
}

FVector UPrimitiveComponent::GetComponentVelocity() const
{
	if (IsSimulatingPhysics())
	{
		FBodyInstance* BodyInst = GetBodyInstance();
		if(BodyInst != NULL)
		{
			return BodyInst->GetUnrealWorldVelocity();
		}
	}

	return Super::GetComponentVelocity();
}

void UPrimitiveComponent::SetCollisionObjectType(ECollisionChannel Channel)
{
	BodyInstance.SetObjectType(Channel);
}

void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
	BodyInstance.SetResponseToChannel(Channel, NewResponse);
	OnComponentCollisionSettingsChanged();
}

void UPrimitiveComponent::SetCollisionResponseToAllChannels(enum ECollisionResponse NewResponse)
{
	BodyInstance.SetResponseToAllChannels(NewResponse);
	OnComponentCollisionSettingsChanged();
}

void UPrimitiveComponent::SetCollisionResponseToChannels(const FCollisionResponseContainer& NewReponses)
{
	BodyInstance.SetResponseToChannels(NewReponses);
	OnComponentCollisionSettingsChanged();
}

void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType)
{
	if (BodyInstance.GetCollisionEnabled() != NewType)
	{
		BodyInstance.SetCollisionEnabled(NewType);

		EnsurePhysicsStateCreated();
		OnComponentCollisionSettingsChanged();

	}
}

// @todo : implement skeletalmeshcomponent version
void UPrimitiveComponent::SetCollisionProfileName(FName InCollisionProfileName)
{
	ECollisionEnabled::Type OldCollisionEnabled = BodyInstance.GetCollisionEnabled();
	BodyInstance.SetCollisionProfileName(InCollisionProfileName);
	OnComponentCollisionSettingsChanged();

	ECollisionEnabled::Type NewCollisionEnabled = BodyInstance.GetCollisionEnabled();

	if (OldCollisionEnabled != NewCollisionEnabled)
	{
		EnsurePhysicsStateCreated();
	}
}

FName UPrimitiveComponent::GetCollisionProfileName()
{
	return BodyInstance.GetCollisionProfileName();
}

void UPrimitiveComponent::OnActorEnableCollisionChanged()
{
	BodyInstance.UpdatePhysicsFilterData();
	OnComponentCollisionSettingsChanged();
}

void UPrimitiveComponent::OnComponentCollisionSettingsChanged()
{
	if (!IsTemplate() && IsRegistered())			// not for CDOs
	{
		// changing collision settings could affect touching status, need to update
		UpdateOverlaps();

		// update navigation data if needed
		const bool bNewNavRelevant = IsNavigationRelevant();
		if (bNavigationRelevant != bNewNavRelevant)
		{
			bNavigationRelevant = bNewNavRelevant;
			UNavigationSystem::UpdateNavOctree(this);
		}
	}
}

bool UPrimitiveComponent::K2_LineTraceComponent(FVector TraceStart, FVector TraceEnd, bool bTraceComplex, bool bShowTrace, FVector& HitLocation, FVector& HitNormal, FName& BoneName)
{
	FHitResult Hit;
	static FName KismetTraceComponentName(TEXT("KismetTraceComponent"));
	FCollisionQueryParams LineParams(KismetTraceComponentName, bTraceComplex);
	const bool bDidHit = LineTraceComponent(Hit, TraceStart, TraceEnd, LineParams);

	if( bDidHit )
	{
		// Fill in the results if we hit
		HitLocation = Hit.Location;
		HitNormal = Hit.Normal;
		BoneName = Hit.BoneName;
	}
	else
	{
		// Blank these out to avoid confusion!
		HitLocation = FVector::ZeroVector;
		HitNormal = FVector::ZeroVector;
		BoneName = NAME_None;
	}

	if( bShowTrace )
	{
		GetWorld()->LineBatcher->DrawLine(TraceStart, bDidHit ? HitLocation : TraceEnd, FLinearColor(1.f,0.5f,0.f), SDPG_World, 2.f);
		if( bDidHit )
		{
			GetWorld()->LineBatcher->DrawLine(HitLocation, TraceEnd, FLinearColor(0.f,0.5f,1.f), SDPG_World, 2.f);
		}
	}

	return bDidHit;
}


ECollisionEnabled::Type UPrimitiveComponent::GetCollisionEnabled() const
{
	return BodyInstance.GetCollisionEnabled();
}

ECollisionResponse UPrimitiveComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	return BodyInstance.GetResponseToChannel(Channel);
}

const FCollisionResponseContainer& UPrimitiveComponent::GetCollisionResponseToChannels() const
{
	return BodyInstance.GetResponseToChannels();
}

void UPrimitiveComponent::UpdatePhysicsToRBChannels()
{
	if(BodyInstance.IsValidBodyInstance())
	{
		BodyInstance.UpdatePhysicsFilterData();
	}
}

#undef LOCTEXT_NAMESPACE