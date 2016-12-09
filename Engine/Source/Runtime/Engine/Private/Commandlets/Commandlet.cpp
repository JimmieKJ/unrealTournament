// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Commandlet.h"

UCommandlet::UCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsServer = true;
	IsClient = true;
	IsEditor = true;
	ShowErrorCount = true;
}
