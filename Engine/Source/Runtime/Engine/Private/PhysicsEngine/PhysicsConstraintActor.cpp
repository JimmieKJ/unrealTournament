// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysXSupport.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"

APhysicsConstraintActor::APhysicsConstraintActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> KBSJointTexture;
		FName NAME_Physics;
		FConstructorStatics()
			: KBSJointTexture(TEXT("/Engine/EditorResources/S_KBSJoint"))
			, NAME_Physics(TEXT("Physics"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstraintComp = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("MyConstraintComp"));
	RootComponent = ConstraintComp;
	bHidden = true;
}

void APhysicsConstraintActor::PostLoad()
{
	Super::PostLoad();

	// Copy 'actors to constrain' into component
	if (GetLinkerUE4Version() < VER_UE4_ALL_PROPS_TO_CONSTRAINTINSTANCE && ConstraintComp != NULL)
	{
		ConstraintComp->ConstraintActor1 = ConstraintActor1_DEPRECATED;
		ConstraintComp->ConstraintActor2 = ConstraintActor2_DEPRECATED;
		ConstraintComp->ConstraintInstance.bDisableCollision = bDisableCollision_DEPRECATED;
	}
}

#if WITH_EDITOR
void APhysicsConstraintActor::LoadedFromAnotherClass( const FName& OldClassName )
{
	Super::LoadedFromAnotherClass(OldClassName);

	static const FName PhysicsBSJointActor_NAME(TEXT("PhysicsBSJointActor"));
	static const FName PhysicsHingeActor_NAME(TEXT("PhysicsHingeActor"));
	static const FName PhysicsPrismaticActor_NAME(TEXT("PhysicsPrismaticActor"));

	if (OldClassName == PhysicsHingeActor_NAME)
	{
		ConstraintComp->ConstraintInstance.ConfigureAsHinge(false);
	}
	else if (OldClassName == PhysicsPrismaticActor_NAME)
	{
		ConstraintComp->ConstraintInstance.ConfigureAsPrismatic(false);
	}
	else if (OldClassName == PhysicsBSJointActor_NAME)
	{
		ConstraintComp->ConstraintInstance.ConfigureAsBS(false);
	}

	ConstraintComp->UpdateSpriteTexture();
}
#endif // WITH_EDITOR

/** Returns ConstraintComp subobject **/
UPhysicsConstraintComponent* APhysicsConstraintActor::GetConstraintComp() const { return ConstraintComp; }
