// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Kismet/BlueprintAsyncActionBase.h"

//////////////////////////////////////////////////////////////////////////
// UBlueprintAsyncActionBase

UBlueprintAsyncActionBase::UBlueprintAsyncActionBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFlags(RF_StrongRefOnFrame);
}

void UBlueprintAsyncActionBase::Activate()
{
}
