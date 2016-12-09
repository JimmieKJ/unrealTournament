// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MacPlatformFile.h"
#include "Apple/ApplePlatformFile.h"

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FApplePlatformFile MacPlatformSingleton;
	return MacPlatformSingleton;
}
