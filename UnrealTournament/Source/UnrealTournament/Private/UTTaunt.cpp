// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTTaunt.h"

AUTTaunt::AUTTaunt(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = TEXT("Unnamed Taunt");
	TauntAuthor = TEXT("Anonymous");
}
