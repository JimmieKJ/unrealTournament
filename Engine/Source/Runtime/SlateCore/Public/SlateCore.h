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

// Compile all the RichText and MultiLine editable text?
#define WITH_FANCY_TEXT 1

/* Forward declarations
 *****************************************************************************/
class FActiveTimerHandle;
enum class EActiveTimerReturnType : uint8;

/* Delegates
 *****************************************************************************/

#include "WidgetActiveTimerDelegate.h"

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
