// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Net/OnlineBlueprintCallProxyBase.h"

//////////////////////////////////////////////////////////////////////////
// UOnlineBlueprintCallProxyBase

UOnlineBlueprintCallProxyBase::UOnlineBlueprintCallProxyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFlags(RF_StrongRefOnFrame);
}

void UOnlineBlueprintCallProxyBase::Activate()
{
}
