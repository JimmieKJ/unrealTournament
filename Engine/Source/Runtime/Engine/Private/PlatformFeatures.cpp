// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

TSharedPtr<const class FJsonObject> IPlatformFeaturesModule::GetTitleSettings()
{
	return nullptr;
}

IVideoRecordingSystem* IPlatformFeaturesModule::GetVideoRecordingSystem()
{
	static FGenericVideoRecordingSystem GenericVideoRecordingSystem;
	return &GenericVideoRecordingSystem;
}
