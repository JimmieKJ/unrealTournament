// Copyright 1998 - 2015 Epic Games, Inc.All Rights Reserved.
#pragma once

#include "UTNavigationSystem.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTNavigationSystem : public UNavigationSystem
{
	GENERATED_BODY()
public:
	UUTNavigationSystem(const FObjectInitializer& OI)
		: Super(OI)
	{
		bAllowClientSideNavigation = true;
	}
};