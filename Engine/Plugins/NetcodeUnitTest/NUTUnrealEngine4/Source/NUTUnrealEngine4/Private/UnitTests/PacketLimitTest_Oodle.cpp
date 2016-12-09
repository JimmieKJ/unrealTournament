// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnitTests/PacketLimitTest_Oodle.h"



/**
 * UPacketLimitTest_Oodle
 */
UPacketLimitTest_Oodle::UPacketLimitTest_Oodle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnitTestName = TEXT("PacketLimitTest_Oodle");

	UnitTestDate = FDateTime(2016, 1, 10);

	bWorkInProgress = true;

	// @todo #JohnBExploitCL: Bugtracking/changelist notes

	ExpectedResult.Add(TEXT("ShooterGame"), EUnitTestVerification::VerifiedNotFixed);


	bUseOodle = true;
}
