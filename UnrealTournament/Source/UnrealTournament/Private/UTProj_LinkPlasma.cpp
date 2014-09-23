// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_LinkPlasma.h"

AUTProj_LinkPlasma::AUTProj_LinkPlasma(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bNetTemporary = true;
}