// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/CameraBlockingVolume.h"
#include "Components/BrushComponent.h"

ACameraBlockingVolume::ACameraBlockingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
}
