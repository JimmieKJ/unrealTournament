// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavigationPathGenerator.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavigationPathGenerator : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavigationPathGenerator
{
	GENERATED_IINTERFACE_BODY()


	/** 
	 *	Retrieved path generated for specified navigation Agent
	 */
	virtual FNavPathSharedPtr GetGeneratedPath(class INavAgentInterface* Agent) PURE_VIRTUAL(INavigationPathGenerator::GetGeneratedPath, return FNavPathSharedPtr(NULL););
};
