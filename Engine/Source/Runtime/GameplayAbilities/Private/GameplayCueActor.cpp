// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayCueActor.h"

AGameplayCueActor::AGameplayCueActor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AttachToOwner = false;
}

void AGameplayCueActor::BeginPlay()
{
	Super::BeginPlay();

	if (AttachToOwner)
	{
		this->AttachRootComponentToActor(GetOwner(), AttachSocketName, EAttachLocation::SnapToTarget);
	}
}