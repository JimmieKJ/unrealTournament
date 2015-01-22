// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	LightingTools.h: Lighting Tools helper
================================================================================*/

#ifndef __LightingTools_h__
#define __LightingTools_h__

#pragma once

/**
 *	LightingTools settings
 */
class FLightingToolsSettings
{

public:

	/** Static: Returns global lighting tools adjust settings */
	static FLightingToolsSettings& Get()
	{
		return LightingToolsSettings;
	}

	static void Init();

	static bool ApplyToggle();

	static void Reset();

protected:

	/** Static: Global lightmap resolution ratio adjust settings */
	static FLightingToolsSettings LightingToolsSettings;

public:

	/** Constructor */
	FLightingToolsSettings() :
		  bShowLightingBounds(false)
		, bShowShadowTraces(false)
		, bShowDirectOnly(false)
		, bShowIndirectOnly(false)
		, bShowIndirectSamples(false)
		, bSavedShowSelection(false)
	{
	}

	bool bShowLightingBounds;
	bool bShowShadowTraces;
	bool bShowDirectOnly;
	bool bShowIndirectOnly;
	bool bShowIndirectSamples;

	bool bSavedShowSelection;
};

#endif	// __LightingTools_h__
