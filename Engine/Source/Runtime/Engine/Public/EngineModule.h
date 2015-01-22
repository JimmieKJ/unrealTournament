// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// 
// Engine module class

#pragma once

class IRendererModule;

/** Implements the engine module. */
class FEngineModule : public FDefaultModuleImpl
{
public:

	// IModuleInterface
	virtual void StartupModule();
};

/** Accessor that gets the renderer module and caches the result. */
extern ENGINE_API IRendererModule& GetRendererModule();

/** Clears the cached renderer module reference. */
extern ENGINE_API void ResetCachedRendererModule();