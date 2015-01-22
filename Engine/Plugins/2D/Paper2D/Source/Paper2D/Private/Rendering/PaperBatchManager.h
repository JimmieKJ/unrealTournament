// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FPaperBatchManager
{
public:
	static void Initialize();
	static void Shutdown();

	static class UPaperBatchComponent* GetBatchComponent(UWorld* World);

	// Gets the batcher for the specified scene
	// Note: Can only be called on the render thread
	static class FPaperBatchSceneProxy* GetBatcher(FSceneInterface& Scene);
private:
	static void OnWorldCreated(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnWorldDestroyed(UWorld* World);
};
