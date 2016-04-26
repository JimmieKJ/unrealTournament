// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "UnrealFrontendMain.h"
#include "RequiredProgramMainCPPInclude.h"
#include "LinuxCommonStartup.h"

int main(int argc, char *argv[])
{
	return CommonLinuxMain(argc, argv, &UnrealFrontendMain);
}
