// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DestructibleActor.cpp: ADestructibleActor methods.
=============================================================================*/


#include "EnginePrivate.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "PhysicsEngine/DestructibleActor.h"
#include "Components/DestructibleComponent.h"

ADestructibleActor::ADestructibleActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DestructibleComponent = CreateDefaultSubobject<UDestructibleComponent>(TEXT("DestructibleComponent0"));
	DestructibleComponent->bCanEverAffectNavigation = bAffectNavigation;
	RootComponent = DestructibleComponent;
}

#if WITH_EDITOR
bool ADestructibleActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	Super::GetReferencedContentObjects(Objects);

	if (DestructibleComponent && DestructibleComponent->SkeletalMesh)
	{
		Objects.Add(DestructibleComponent->SkeletalMesh);
	}
	return true;
}
#endif // WITH_EDITOR



/** Returns DestructibleComponent subobject **/
UDestructibleComponent* ADestructibleActor::GetDestructibleComponent() const { return DestructibleComponent; }
