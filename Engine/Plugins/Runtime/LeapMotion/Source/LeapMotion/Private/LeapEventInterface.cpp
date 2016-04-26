// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LeapMotionPrivatePCH.h"
#include "LeapEventInterface.h"


ULeapEventInterface::ULeapEventInterface(const class FObjectInitializer& Init)
	: Super(Init)
{

}

FString ILeapEventInterface::ToString()
{
	return "ILeapEventInterface::ToString()";
}