// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "SlateCore.h"

class ISlate3DRenderer;
class ISlateFontAtlasFactory;

/**
 * Interface for the Slate RHI Renderer module.
 */
class ISlateRHIRendererModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a Slate RHI renderer.
	 *
	 * @return A new renderer.
	 */
	virtual TSharedRef<FSlateRenderer> CreateSlateRHIRenderer( ) = 0;

	virtual TSharedRef<ISlate3DRenderer> CreateSlate3DRenderer() = 0;

	virtual TSharedRef<ISlateFontAtlasFactory> CreateSlateFontAtlasFactory() = 0;
};
