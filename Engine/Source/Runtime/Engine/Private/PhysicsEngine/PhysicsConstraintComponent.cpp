// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysXSupport.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "ConstraintComponent"

UPhysicsConstraintComponent::UPhysicsConstraintComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}



UPrimitiveComponent* UPhysicsConstraintComponent::GetComponentInternal(EConstraintFrame::Type Frame) const
{
	UPrimitiveComponent* PrimComp = NULL;

	FName ComponentName = NAME_None;
	AActor* Actor = NULL;

	// Frame 1
	if(Frame == EConstraintFrame::Frame1)
	{
		// Use override component if specified
		if(OverrideComponent1.IsValid())
		{
			return OverrideComponent1.Get();
		}

		ComponentName = ComponentName1.ComponentName;
		Actor = ConstraintActor1;
	}
	// Frame 2
	else
	{
		// Use override component if specified
		if(OverrideComponent2.IsValid())
		{
			return OverrideComponent2.Get();
		}

		ComponentName = ComponentName2.ComponentName;
		Actor = ConstraintActor2;
	}

	// If neither actor nor component name specified, joint to 'world'
	if(Actor != NULL || ComponentName != NAME_None)
	{
		// If no actor specified, but component name is - use Owner
		if(Actor == NULL)
		{
			Actor = GetOwner();
		}

		// If we now have an Actor, lets find a component
		if(Actor != NULL)
		{
			// No name specified, use the root component
			if(ComponentName == NAME_None)
			{
				PrimComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
			}
			// Name specified, see if we can find that property..
			else
			{
				UObjectPropertyBase* ObjProp = FindField<UObjectPropertyBase>(Actor->GetClass(), ComponentName);
				if(ObjProp != NULL)
				{
					// .. and return the component that is there
					PrimComp = Cast<UPrimitiveComponent>(ObjProp->GetObjectPropertyValue_InContainer(Actor));
				}
			}
		}	
	}

	return PrimComp;
}

FTransform UPhysicsConstraintComponent::GetBodyTransformInternal(EConstraintFrame::Type Frame, FName InBoneName) const
{
	UPrimitiveComponent* PrimComp = GetComponentInternal(Frame);
	if(!PrimComp)
	{
		return FTransform::Identity;
	}
	  
	//Use ComponentToWorld by default for all components
	FTransform ResultTM = PrimComp->ComponentToWorld;
		
	// Skeletal case
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp);
	if(SkelComp != NULL)
	{
		int32 BoneIndex = SkelComp->GetBoneIndex(InBoneName);
		if(BoneIndex != INDEX_NONE)
		{	
			ResultTM = SkelComp->GetBoneTransform(BoneIndex);
		}
	}

	return ResultTM;
}

FBox UPhysicsConstraintComponent::GetBodyBoxInternal(EConstraintFrame::Type Frame, FName InBoneName) const
{
	FBox ResultBox(0);

	UPrimitiveComponent* PrimComp  = GetComponentInternal(Frame);

	// Skeletal case
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp);
	if(SkelComp != NULL)
	{
		UPhysicsAsset * const PhysicsAsset = SkelComp->GetPhysicsAsset();
		if (PhysicsAsset)
		{
			int32 BoneIndex = SkelComp->GetBoneIndex(InBoneName);
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(InBoneName);
			if(BoneIndex != INDEX_NONE && BodyIndex != INDEX_NONE)
			{	
				const FTransform BoneTransform = SkelComp->GetBoneTransform(BoneIndex);
				ResultBox = PhysicsAsset->BodySetup[BodyIndex]->AggGeom.CalcAABB(BoneTransform);
			}
		}
	}
	else if(PrimComp != NULL)
	{
		ResultBox = PrimComp->Bounds.GetBox();
	}

	return ResultBox;
}

FTransform UPhysicsConstraintComponent::GetBodyTransform(EConstraintFrame::Type Frame) const
{
	if(Frame == EConstraintFrame::Frame1)
	{
		return GetBodyTransformInternal(Frame, ConstraintInstance.ConstraintBone1);
	}
	else
	{
		return GetBodyTransformInternal(Frame, ConstraintInstance.ConstraintBone2);
	}
}

FBox UPhysicsConstraintComponent::GetBodyBox(EConstraintFrame::Type Frame) const
{
	if(Frame == EConstraintFrame::Frame1)
	{
		return GetBodyBoxInternal(Frame, ConstraintInstance.ConstraintBone1);
	}
	else
	{
		return GetBodyBoxInternal(Frame, ConstraintInstance.ConstraintBone2);
	}
}

FBodyInstance* UPhysicsConstraintComponent::GetBodyInstance(EConstraintFrame::Type Frame) const
{
	FBodyInstance* Instance = NULL;
	UPrimitiveComponent* PrimComp = GetComponentInternal(Frame);
	if(PrimComp != NULL)
	{
		if(Frame == EConstraintFrame::Frame1)
		{
			Instance = PrimComp->GetBodyInstance(ConstraintInstance.ConstraintBone1);
		}
		else
		{
			Instance = PrimComp->GetBodyInstance(ConstraintInstance.ConstraintBone2);
		}
	}
	return Instance;
}


void UPhysicsConstraintComponent::InitComponentConstraint()
{
	// First we convert world space position of constraint into local space frames
	UpdateConstraintFrames();

	// Then we init the constraint
	FBodyInstance* Body1 = GetBodyInstance(EConstraintFrame::Frame1);
	FBodyInstance* Body2 = GetBodyInstance(EConstraintFrame::Frame2);
	ConstraintInstance.InitConstraint(this, Body1, Body2, 1.0f);
}

void UPhysicsConstraintComponent::TermComponentConstraint()
{
	ConstraintInstance.TermConstraint();
}

void UPhysicsConstraintComponent::OnConstraintBrokenHandler(FConstraintInstance* BrokenConstraint)
{
	OnConstraintBroken.Broadcast(BrokenConstraint->ConstraintIndex);
}

void UPhysicsConstraintComponent::SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2)
{
	if(Component1 != NULL)
	{
		this->ComponentName1.ComponentName = Component1->GetFName();
		OverrideComponent1 = Component1;
		ConstraintInstance.ConstraintBone1 = BoneName1;
	}

	if(Component2 != NULL)
	{
		this->ComponentName2.ComponentName = Component2->GetFName();
		OverrideComponent2 = Component2;
		ConstraintInstance.ConstraintBone2 = BoneName2;
	}

	InitComponentConstraint();
}

void UPhysicsConstraintComponent::BreakConstraint()
{
	ConstraintInstance.TermConstraint();
}


void UPhysicsConstraintComponent::InitializeComponent()
{
	Super::InitializeComponent();
	InitComponentConstraint();
}

#if WITH_EDITOR
void UPhysicsConstraintComponent::OnRegister()
{
	Super::OnRegister();

	if (SpriteComponent)
	{
		UpdateSpriteTexture();
		SpriteComponent->SpriteInfo.Category = TEXT("Physics");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT( "SpriteCategory", "Physics", "Physics" );
	}
}
#endif

void UPhysicsConstraintComponent::OnUnregister()
{
	Super::OnUnregister();

	// Slight hack - there isn't an EndPlayComponent, so we see if we are unregistered and we have an owner but its gone, and if so, we shut down constraint
	if(GetOwner() && GetOwner()->IsPendingKillPending())
	{
		TermComponentConstraint();
	}
}

void UPhysicsConstraintComponent::BeginDestroy()
{
	Super::BeginDestroy();
	TermComponentConstraint();
}

void UPhysicsConstraintComponent::PostLoad()
{
	Super::PostLoad();

	// Fix old content that used a ConstraintSetup
	if ( GetLinkerUE4Version() < VER_UE4_ALL_PROPS_TO_CONSTRAINTINSTANCE && (ConstraintSetup_DEPRECATED != NULL) )
	{
		// Will have copied from setup into DefaultIntance inside
		ConstraintInstance.CopyConstraintParamsFrom(&ConstraintSetup_DEPRECATED->DefaultInstance);
		ConstraintSetup_DEPRECATED = NULL;
	}

	if (GetLinkerUE4Version() < VER_UE4_SOFT_CONSTRAINTS_USE_MASS)
	{
		//In previous versions the mass was placed into the spring constant. This is correct because you use different springs for different mass - however, this makes tuning hard
		//We now multiply mass into the spring constant. To fix old data we use CalculateMass which is not perfect but close (within 0.1kg)
		//We also use the primitive body instance directly for determining if simulated - this is potentially wrong for fixed bones in skeletal mesh, but it's much more likely right (in skeletal case we don't have access to bodies to check)
		
		UPrimitiveComponent * Primitive1 = GetComponentInternal(EConstraintFrame::Frame1);
		UPrimitiveComponent * Primitive2 = GetComponentInternal(EConstraintFrame::Frame2);
		
		int NumDynamic = 0;
		float TotalMass = 0.f;

		if (Primitive1 && Primitive1->BodyInstance.bSimulatePhysics)
		{
			FName BoneName = ConstraintInstance.ConstraintBone1;
			++NumDynamic;
			TotalMass += Primitive1->CalculateMass(BoneName);
		}

		if (Primitive2 && Primitive2->BodyInstance.bSimulatePhysics)
		{
			FName BoneName = ConstraintInstance.ConstraintBone2;
			++NumDynamic;
			TotalMass += Primitive2->CalculateMass(BoneName);
		}

		if ( (NumDynamic > 0) && (TotalMass > 0) )	//we don't support cases where both constrained bodies are static or NULL, but add this anyway to avoid crash
		{
			float AverageMass = TotalMass / NumDynamic;

			ConstraintInstance.LinearLimitStiffness /= AverageMass;
			ConstraintInstance.SwingLimitStiffness /= AverageMass;
			ConstraintInstance.TwistLimitStiffness /= AverageMass;
			ConstraintInstance.LinearLimitDamping /= AverageMass;
			ConstraintInstance.SwingLimitDamping /= AverageMass;
			ConstraintInstance.TwistLimitDamping /= AverageMass;
		}

	}
}

#if WITH_EDITOR

void UPhysicsConstraintComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateConstraintFrames();
	UpdateSpriteTexture();
}

void UPhysicsConstraintComponent::PostEditComponentMove(bool bFinished)
{
	Super::PostEditComponentMove(bFinished);

	// Update frames
	UpdateConstraintFrames();
}

void UPhysicsConstraintComponent::CheckForErrors()
{
	Super::CheckForErrors();

	UPrimitiveComponent* PrimComp1 = GetComponentInternal(EConstraintFrame::Frame1);
	UPrimitiveComponent* PrimComp2 = GetComponentInternal(EConstraintFrame::Frame2);

	// Check we have something to joint
	if( PrimComp1 == NULL && PrimComp2 == NULL )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format( LOCTEXT("NoComponentsFound","{OwnerName} : No components found to joint."), Arguments ) ));
	}
	// Make sure constraint components are not both static.
	else if ( PrimComp1 != NULL && PrimComp2 != NULL )
	{
		if ( PrimComp1->Mobility != EComponentMobility::Movable && PrimComp2->Mobility != EComponentMobility::Movable )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT("BothComponentsStatic","{OwnerName} : Both components are static."), Arguments ) ));
		}
	}
	else
	{
		// At this point, we know one constraint component is NULL and the other is non-NULL.
		// Check that the non-NULL constraint component is dynamic.
		if ( ( PrimComp1 == NULL && PrimComp2->Mobility != EComponentMobility::Movable ) ||
			( PrimComp2 == NULL && PrimComp1->Mobility != EComponentMobility::Movable ) )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwnerName"), FText::FromString(GetNameSafe(GetOwner())));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT("SingleStaticComponent","{OwnerName} : Connected to single static component."), Arguments ) ));
		}
	}
}

#endif // WITH_EDITOR

void UPhysicsConstraintComponent::UpdateConstraintFrames()
{
	FTransform A1Transform = GetBodyTransform(EConstraintFrame::Frame1);
	A1Transform.SetScale3D(FVector(1.f));
	FTransform A2Transform = GetBodyTransform(EConstraintFrame::Frame2);
	A2Transform.SetScale3D(FVector(1.f));

	// World ref frame
	const FVector WPos = GetComponentLocation();
	const FVector WPri = ComponentToWorld.GetUnitAxis( EAxis::X );
	const FVector WOrth = ComponentToWorld.GetUnitAxis( EAxis::Y );

	ConstraintInstance.Pos1 = A1Transform.InverseTransformPosition(WPos);
	ConstraintInstance.PriAxis1 = A1Transform.InverseTransformVectorNoScale(WPri);
	ConstraintInstance.SecAxis1 = A1Transform.InverseTransformVectorNoScale(WOrth);

	const FVector RotatedX = ConstraintInstance.AngularRotationOffset.RotateVector(FVector(1,0,0));
	const FVector RotatedY = ConstraintInstance.AngularRotationOffset.RotateVector(FVector(0,1,0));
	const FVector WPri2 = ComponentToWorld.TransformVector(RotatedX);
	const FVector WOrth2 = ComponentToWorld.TransformVector(RotatedY);
	ConstraintInstance.Pos2 = A2Transform.InverseTransformPosition(WPos);
	ConstraintInstance.PriAxis2 = A2Transform.InverseTransformVectorNoScale(WPri2);
	ConstraintInstance.SecAxis2 = A2Transform.InverseTransformVectorNoScale(WOrth2);
}

void UPhysicsConstraintComponent::SetConstraintReferenceFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame)
{
	ConstraintInstance.SetRefFrame(Frame, RefFrame);
}

void UPhysicsConstraintComponent::SetConstraintReferencePosition(EConstraintFrame::Type Frame, const FVector& RefPosition)
{
	ConstraintInstance.SetRefPosition(Frame, RefPosition);
}

void UPhysicsConstraintComponent::SetConstraintReferenceOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis)
{
	ConstraintInstance.SetRefOrientation(Frame, PriAxis, SecAxis);
}

void UPhysicsConstraintComponent::GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce)
{
	ConstraintInstance.GetConstraintForce(OutLinearForce, OutAngularForce);
}

void UPhysicsConstraintComponent::SetDisableCollision(bool bDisableCollision)
{
	ConstraintInstance.SetDisableCollision(bDisableCollision);
}


#if WITH_EDITOR
void UPhysicsConstraintComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		if (ConstraintInstance.IsHinge())
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KHinge.S_KHinge")));
		}
		else if (ConstraintInstance.IsPrismatic())
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KPrismatic.S_KPrismatic")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KBSJoint.S_KBSJoint")));
		}
	}
}
#endif // WITH_EDITOR

void UPhysicsConstraintComponent::SetLinearPositionDrive( bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ )
{
	ConstraintInstance.SetLinearPositionDrive(bEnableDriveX, bEnableDriveY, bEnableDriveZ);
}

void UPhysicsConstraintComponent::SetLinearVelocityDrive( bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ )
{
	ConstraintInstance.SetLinearVelocityDrive(bEnableDriveX, bEnableDriveY, bEnableDriveZ);
}

void UPhysicsConstraintComponent::SetAngularOrientationDrive( bool bEnableSwingDrive, bool bEnableTwistDrive )
{
	ConstraintInstance.SetAngularPositionDrive(bEnableSwingDrive, bEnableTwistDrive);
}

void UPhysicsConstraintComponent::SetAngularVelocityDrive( bool bEnableSwingDrive, bool bEnableTwistDrive )
{
	ConstraintInstance.SetAngularVelocityDrive(bEnableSwingDrive, bEnableTwistDrive);
}

void UPhysicsConstraintComponent::SetLinearPositionTarget( const FVector& InPosTarget )
{
	ConstraintInstance.SetLinearPositionTarget(InPosTarget);
}

void UPhysicsConstraintComponent::SetLinearVelocityTarget( const FVector& InVelTarget )
{
	ConstraintInstance.SetLinearVelocityTarget(InVelTarget);
}

void UPhysicsConstraintComponent::SetLinearDriveParams( float InSpring, float InDamping, float InForceLimit )
{
	ConstraintInstance.SetLinearDriveParams(InSpring, InDamping, InForceLimit);
}

void UPhysicsConstraintComponent::SetAngularOrientationTarget( const FRotator& InPosTarget )
{
	ConstraintInstance.SetAngularOrientationTarget(InPosTarget.Quaternion());
}

void UPhysicsConstraintComponent::SetAngularVelocityTarget( const FVector& InVelTarget )
{
	ConstraintInstance.SetAngularVelocityTarget(InVelTarget);
}

void UPhysicsConstraintComponent::SetAngularDriveParams( float InSpring, float InDamping, float InForceLimit )
{
	ConstraintInstance.SetAngularDriveParams(InSpring, InDamping, InForceLimit);
}

void UPhysicsConstraintComponent::SetLinearXLimit(ELinearConstraintMotion Motion, float LinearLimit)
{
	ConstraintInstance.SetLinearXLimit(Motion, LinearLimit);
}

void UPhysicsConstraintComponent::SetLinearYLimit(ELinearConstraintMotion Motion, float LinearLimit)
{
	ConstraintInstance.SetLinearYLimit(Motion, LinearLimit);
}

void UPhysicsConstraintComponent::SetLinearZLimit(ELinearConstraintMotion Motion, float LinearLimit)
{
	ConstraintInstance.SetLinearZLimit(Motion, LinearLimit);
}

void UPhysicsConstraintComponent::SetAngularSwing1Limit(EAngularConstraintMotion Motion, float Swing1LimitAngle)
{
	ConstraintInstance.SetAngularSwing1Limit(Motion, Swing1LimitAngle);
}

void UPhysicsConstraintComponent::SetAngularSwing2Limit(EAngularConstraintMotion Motion, float Swing2LimitAngle)
{
	ConstraintInstance.SetAngularSwing2Limit(Motion, Swing2LimitAngle);
}

void UPhysicsConstraintComponent::SetAngularTwistLimit(EAngularConstraintMotion Motion, float TwistLimitAngle)
{
	ConstraintInstance.SetAngularTwistLimit(Motion, TwistLimitAngle);
}

float UPhysicsConstraintComponent::GetCurrentTwist() const
{
	const float CurrentTwistRads = ConstraintInstance.GetCurrentTwist();
	return FMath::RadiansToDegrees(CurrentTwistRads);
}

float UPhysicsConstraintComponent::GetCurrentSwing1() const
{
	const float CurrentSwing1Rads = ConstraintInstance.GetCurrentSwing1();
	return FMath::RadiansToDegrees(CurrentSwing1Rads);
}

float UPhysicsConstraintComponent::GetCurrentSwing2() const
{
	const float CurrentSwing2Rads = ConstraintInstance.GetCurrentSwing2();
	return FMath::RadiansToDegrees(CurrentSwing2Rads);
}

#undef LOCTEXT_NAMESPACE
