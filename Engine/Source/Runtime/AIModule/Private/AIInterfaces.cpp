// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "AIResourceInterface.h"
#include "GenericTeamAgentInterface.h"

UAIResourceInterface::UAIResourceInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGenericTeamId FGenericTeamId::NoTeam(FGenericTeamId::NoTeamId);

FGenericTeamId FGenericTeamId::GetTeamIdentifier(const AActor* TeamMember)
{
	const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(TeamMember);
	if (TeamAgent)
	{
		return TeamAgent->GetGenericTeamId();
	}

	return FGenericTeamId::NoTeam;
}

ETeamAttitude::Type FGenericTeamId::GetAttitude(const AActor* A, const AActor* B)
{
	const IGenericTeamAgentInterface* TeamAgentA = Cast<const IGenericTeamAgentInterface>(A);

	return TeamAgentA == NULL || B == NULL ? ETeamAttitude::Neutral : TeamAgentA->GetTeamAttitudeTowards(*B);
}

UGenericTeamAgentInterface::UGenericTeamAgentInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
