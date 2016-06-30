// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_InstantReplay.h"

AUTHUD_InstantReplay::AUTHUD_InstantReplay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

EInputMode::Type AUTHUD_InstantReplay::GetInputMode_Implementation() const
{
	return EInputMode::EIM_GameOnly;
}