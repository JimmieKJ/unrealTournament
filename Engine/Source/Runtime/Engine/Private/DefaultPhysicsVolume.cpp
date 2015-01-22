// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "GameFramework/DefaultPhysicsVolume.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolume, Log, All);

ADefaultPhysicsVolume::ADefaultPhysicsVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

#if WITH_EDITORONLY_DATA
	// Not allowed to be selected or edited within Unreal Editor
	bEditable = false;
#endif // WITH_EDITORONLY_DATA
}

void ADefaultPhysicsVolume::Destroyed()
{
	UE_LOG(LogVolume, Log, TEXT("%s destroyed!"), *GetName());
}
