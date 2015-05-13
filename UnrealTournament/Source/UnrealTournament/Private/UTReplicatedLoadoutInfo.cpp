// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedLoadoutInfo.h"
#include "Net/UnrealNetwork.h"

AUTReplicatedLoadoutInfo::AUTReplicatedLoadoutInfo(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;
	bNetLoadOnClient = false;
}

void AUTReplicatedLoadoutInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, ItemClass, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, RoundMask, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, bDefaultInclude, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, bPurchaseOnly, COND_InitialOnly);

	DOREPLIFETIME(AUTReplicatedLoadoutInfo, CurrentCost);
}
