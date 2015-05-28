// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "DVRStreaming.h"


ISaveGameSystem* IPlatformFeaturesModule::GetSaveGameSystem()
{
	static FGenericSaveGameSystem GenericSaveGame;
	return &GenericSaveGame;
}


IDVRStreamingSystem* IPlatformFeaturesModule::GetStreamingSystem()
{
	static FGenericDVRStreamingSystem GenericStreamingSystem;
	return &GenericStreamingSystem;
}


