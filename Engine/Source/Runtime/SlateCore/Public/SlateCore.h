// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Dependencies
 *****************************************************************************/

#include "CoreUObject.h"
#include "InputCore.h"
#include "ModuleManager.h"


/* Globals
 *****************************************************************************/

SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlate, Log, All);
SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlateStyles, Log, All);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Measure String"), STAT_SlateMeasureStringTime, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Slate Rendering RT Time"), STAT_SlateRenderingRTTime, STATGROUP_Slate , SLATECORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Slate RT Present Time"), STAT_SlatePresentRTTime, STATGROUP_Slate , SLATECORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Buffers RT"), STAT_SlateUpdateBufferRTTime, STATGROUP_Slate , SLATECORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw Time"), STAT_SlateDrawTime, STATGROUP_Slate, SLATECORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Buffers GT"), STAT_SlateUpdateBufferGTTime, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Font Caching"), STAT_SlateFontCachingTime, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Elements"), STAT_SlateBuildElements, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Find Batch"), STAT_SlateFindBatchTime, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Lines"), STAT_SlateBuildLines, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Splines"), STAT_SlateBuildSplines, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Gradients"), STAT_SlateBuildGradients, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Text"), STAT_SlateBuildText, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Border"), STAT_SlateBuildBorders, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Boxes"), STAT_SlateBuildBoxes, STATGROUP_Slate,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Slate Rendering GT Time"), STAT_SlateRenderingGTTime, STATGROUP_Slate, SLATECORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Build Viewports"), STAT_SlateBuildViewports, STATGROUP_Slate,);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Texture Atlases"), STAT_SlateNumTextureAtlases, STATGROUP_SlateMemory, SLATECORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Font Atlases"), STAT_SlateNumFontAtlases, STATGROUP_SlateMemory,);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Non-Atlased Textures"), STAT_SlateNumNonAtlasedTextures, STATGROUP_SlateMemory,SLATECORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Dynamic Textures"), STAT_SlateNumDynamicTextures, STATGROUP_SlateMemory, SLATECORE_API);

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Layers"), STAT_SlateNumLayers, STATGROUP_Slate,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Batches"), STAT_SlateNumBatches, STATGROUP_Slate,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Vertices"), STAT_SlateVertexCount, STATGROUP_Slate,);

DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture Data CPU Memory"), STAT_SlateTextureDataMemory, STATGROUP_SlateMemory, SLATECORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture Atlas CPU Memory"), STAT_SlateTextureAtlasMemory, STATGROUP_SlateMemory,);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture Data GPU Memory"), STAT_SlateTextureGPUMemory, STATGROUP_SlateMemory, SLATECORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Font Kerning Table Memory"), STAT_SlateFontKerningTableMemory, STATGROUP_SlateMemory,);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Font Measure Memory"), STAT_SlateFontMeasureCacheMemory, STATGROUP_SlateMemory,);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Batch Vertex Memory"), STAT_SlateVertexBatchMemory, STATGROUP_SlateMemory,);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Batch Index Memory"), STAT_SlateIndexBatchMemory, STATGROUP_SlateMemory,);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Vertex Buffer Memory"), STAT_SlateVertexBufferMemory, STATGROUP_SlateMemory, SLATECORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Index Buffer Memory"), STAT_SlateIndexBufferMemory, STATGROUP_SlateMemory, SLATECORE_API);


// Compile all the RichText and MultiLine editable text?
#define WITH_FANCY_TEXT 1


/* Includes
 *****************************************************************************/

// Types
#include "SlateConstants.h"
#include "SlateEnums.h"
#include "SlateStructs.h"
#include "PaintArgs.h"
#include "ISlateMetaData.h"

// Layout
#include "SlotBase.h"
#include "Margin.h"
#include "SlateRect.h"
#include "PaintGeometry.h"
#include "Geometry.h"
#include "Visibility.h"
#include "ArrangedWidget.h"
#include "ArrangedChildren.h"
#include "SNullWidget.h"
#include "Children.h"
#include "WidgetPath.h"
#include "LayoutUtils.h"

// Animation
#include "CurveHandle.h"
#include "CurveSequence.h"
#include "SlateSprings.h"

// Sound
#include "SlateSound.h"
#include "ISlateSoundDevice.h"

// Styling
#include "WidgetStyle.h"
#include "SlateBrush.h"
#include "SlateColor.h"
#include "SlateWidgetStyle.h"
#include "SlateWidgetStyleAsset.h"

// Textures
#include "SlateShaderResource.h"
#include "SlateTextureData.h"
#include "SlateUpdatableTexture.h"
#include "TextureAtlas.h"
#include "TextureManager.h"

// Fonts
#include "SlateFontInfo.h"
#include "FontTypes.h"
#include "FontCache.h"
#include "FontMeasure.h"

// Brushes
#include "SlateBorderBrush.h"
#include "SlateBoxBrush.h"
#include "SlateColorBrush.h"
#include "SlateDynamicImageBrush.h"
#include "SlateImageBrush.h"
#include "SlateNoResource.h"

// Styling (continued)
#include "StyleDefaults.h"
#include "ISlateStyle.h"
#include "SlateStyleRegistry.h"
#include "SlateStyle.h"

// Input
#include "ReplyBase.h"
#include "CursorReply.h"
#include "Events.h"
#include "DragAndDrop.h"
#include "Reply.h"
#include "NavigationReply.h"
#include "NavigationMetaData.h"

// Rendering
#include "RenderingCommon.h"
#include "DrawElements.h"
#include "RenderingPolicy.h"
#include "SlateDrawBuffer.h"
#include "SlateRenderer.h"

// Application
#include "IToolTip.h"
#include "SlateWindowHelper.h"
#include "SlateApplicationBase.h"
#include "ThrottleManager.h"

// Widgets
#include "DeclarativeSyntaxSupport.h"
#include "SWidget.h"
#include "SCompoundWidget.h"
#include "SUserWidget.h"
#include "SLeafWidget.h"
#include "SPanel.h"
#include "WidgetPath.inl"
////
#include "SBoxPanel.h"
#include "SOverlay.h"
#include "SlateTypes.h"
#include "CoreStyle.h"
////
#include "SWindow.h"

//Logging
#include "IEventLogger.h"
#include "EventLogger.h"
