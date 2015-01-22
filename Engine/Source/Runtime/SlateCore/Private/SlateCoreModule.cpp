// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "ModuleManager.h"


DEFINE_LOG_CATEGORY(LogSlate);
DEFINE_LOG_CATEGORY(LogSlateStyles);

DEFINE_STAT(STAT_SlateMeasureStringTime);
DEFINE_STAT(STAT_SlateRenderingRTTime);
DEFINE_STAT(STAT_SlatePresentRTTime);
DEFINE_STAT(STAT_SlateUpdateBufferRTTime);
DEFINE_STAT(STAT_SlateDrawTime);
DEFINE_STAT(STAT_SlateUpdateBufferGTTime);
DEFINE_STAT(STAT_SlateBuildLines);
DEFINE_STAT(STAT_SlateBuildBoxes);
DEFINE_STAT(STAT_SlateBuildBorders);
DEFINE_STAT(STAT_SlateBuildText);
DEFINE_STAT(STAT_SlateBuildGradients);
DEFINE_STAT(STAT_SlateBuildSplines);
DEFINE_STAT(STAT_SlateBuildElements);
DEFINE_STAT(STAT_SlateFindBatchTime);
DEFINE_STAT(STAT_SlateFontCachingTime);
DEFINE_STAT(STAT_SlateRenderingGTTime);
DEFINE_STAT(STAT_SlateBuildViewports);

DEFINE_STAT(STAT_SlateNumTextureAtlases);
DEFINE_STAT(STAT_SlateNumFontAtlases);
DEFINE_STAT(STAT_SlateNumNonAtlasedTextures);
DEFINE_STAT(STAT_SlateNumDynamicTextures);

DEFINE_STAT(STAT_SlateNumLayers);
DEFINE_STAT(STAT_SlateNumBatches);
DEFINE_STAT(STAT_SlateVertexCount);

DEFINE_STAT(STAT_SlateTextureDataMemory);
DEFINE_STAT(STAT_SlateTextureAtlasMemory);
DEFINE_STAT(STAT_SlateTextureGPUMemory);
DEFINE_STAT(STAT_SlateFontKerningTableMemory);
DEFINE_STAT(STAT_SlateFontMeasureCacheMemory);
DEFINE_STAT(STAT_SlateVertexBatchMemory);
DEFINE_STAT(STAT_SlateIndexBatchMemory);
DEFINE_STAT(STAT_SlateVertexBufferMemory);
DEFINE_STAT(STAT_SlateIndexBufferMemory);


const float FOptionalSize::Unspecified = -1.0f;


/**
 * Implements the SlateCore module.
 */
class FSlateCoreModule
	: public IModuleInterface
{
public:

};


IMPLEMENT_MODULE(FSlateCoreModule, SlateCore);
