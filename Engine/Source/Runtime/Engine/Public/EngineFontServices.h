// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateCore.h"

/** 
 * Provides the Engine with access to the Slate font cache and font measuring services (for Canvas)
 * On the game thread this just leverages the Slate services, but the render thread needs its own instances
 */
class ENGINE_API FEngineFontServices
{
public:
	/** Create the singular instance of this class - must be called from the game thread */
	static void Create();

	/** Destroy the singular instance of this class - must be called from the game thread */
	static void Destroy();

	/** Check to see if the singular instance of this class is currently initialized and ready */
	static bool IsInitialized();

	/** Get the singular instance of this class */
	static FEngineFontServices& Get();

	/** Get the font cache to use for the current thread */
	TSharedPtr<FSlateFontCache> GetFontCache();

	/** Get the font measure to use for the current thread */
	TSharedPtr<FSlateFontMeasure> GetFontMeasure();

	/** Update the cache for the current thread */
	void UpdateCache();

private:
	/** Constructor - must be called from the game thread */
	FEngineFontServices();

	/** Destructor - must be called from the game thread */
	~FEngineFontServices();

	/** Create the font cache for the render thread if it doesn't yet exist */
	void ConditionalCreateRenderThreadFontCache();

	/** Create the font measure for the render thread if it doesn't yet exist */
	void ConditionalCreatRenderThreadFontMeasure();

	/** Font atlas factory to use for the render thread - created on the game thread as it depends on another module */
	TSharedPtr<ISlateFontAtlasFactory> RenderThreadFontAtlasFactory;

	/** Font cache used by the render thread - creation is delayed until the first request is made */
	TSharedPtr<FSlateFontCache> RenderThreadFontCache;

	/** Font measure used by the render thread - creation is delayed until the first request is made */
	TSharedPtr<FSlateFontMeasure> RenderThreadFontMeasure;

	/** Singular instance of this class */
	static FEngineFontServices* Instance;
};
