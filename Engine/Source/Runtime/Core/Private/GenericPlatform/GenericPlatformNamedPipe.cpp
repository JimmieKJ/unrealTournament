// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformNamedPipe.h"
#include "Containers/UnrealString.h"


#if PLATFORM_SUPPORTS_NAMED_PIPES

FGenericPlatformNamedPipe::FGenericPlatformNamedPipe()
{
	NamePtr = new FString();
}


FGenericPlatformNamedPipe::~FGenericPlatformNamedPipe()
{
	delete NamePtr;
	NamePtr = NULL;
}

#endif	// PLATFORM_SUPPORTS_NAMED_PIPES
