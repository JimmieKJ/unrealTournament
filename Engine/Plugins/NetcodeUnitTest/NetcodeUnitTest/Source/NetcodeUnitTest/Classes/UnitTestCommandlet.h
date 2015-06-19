// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "UnitTestCommandlet.generated.h"

/**
 * A commandlet for running unit tests, without having to launch the game client.
 * Uses a heavily stripped down game client, with allowances for Slate, and some hacks for enabling proper netcode usage.
 *
 * Usage:
 *	"UE4Editor.exe shootergame -run=NetcodeUnitTest.UnitTestCommandlet"
 *
 * Parameters:
 *	"-UnitTest=UnitTestName"	- Launches only the specified unit test
 */
UCLASS()
class UUnitTestCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	virtual int32 Main(const FString& Params) override;

	virtual void CreateCustomEngine(const FString& Params) override;
};
