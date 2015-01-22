// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorModeRegistry.h"
#include "EditorComponents.h"
#include "UnrealWidget.h"
#include "EngineGlobals.h"

enum EModeTools : int8;
class FEditorViewportClient;
class HHitProxy;
struct FViewportClick;
class FModeTool;

DECLARE_LOG_CATEGORY_EXTERN(LogEditorModes, Log, All);

/** Outcomes when determining whether it's possible to perform an action on the edit modes*/
namespace EEditAction
{
	enum Type
	{
		/** Can't process this action */
		Skip		= 0,
		/** Can process this action */
		Process,
		/** Stop evaluating other modes (early out) */
		Halt,
	};
};

// Builtin editor mode constants
struct UNREALED_API FBuiltinEditorModes
{
public:
	/** Gameplay, editor disabled. */
	const static FEditorModeID EM_None;

	/** Camera movement, actor placement. */
	const static FEditorModeID EM_Default;

	/** Placement mode */
	const static FEditorModeID EM_Placement;

	/** Bsp mode */
	const static FEditorModeID EM_Bsp;

	/** Geometry editing mode. */
	const static FEditorModeID EM_Geometry;

	/** Interpolation editing. */
	const static FEditorModeID EM_InterpEdit;

	/** Texture alignment via the widget. */
	const static FEditorModeID EM_Texture;

	/** Mesh paint tool */
	const static FEditorModeID EM_MeshPaint;

	/** Landscape editing */
	const static FEditorModeID EM_Landscape;

	/** Foliage painting */
	const static FEditorModeID EM_Foliage;

	/** Level editing mode */
	const static FEditorModeID EM_Level;

	/** Streaming level editing mode */
	const static FEditorModeID EM_StreamingLevel;

	/** Physics manipulation mode ( available only when simulating in viewport )*/
	const static FEditorModeID EM_Physics;

	/** Actor picker mode, used to interactively pick actors in the viewport */
	const static FEditorModeID EM_ActorPicker;

private:
	FBuiltinEditorModes() {}
};

/** Material proxy wrapper that can be created on the game thread and passed on to the render thread. */
class UNREALED_API FDynamicColoredMaterialRenderProxy : public FDynamicPrimitiveResource, public FColoredMaterialRenderProxy
{
public:
	/** Initialization constructor. */
	FDynamicColoredMaterialRenderProxy(const FMaterialRenderProxy* InParent,const FLinearColor& InColor)
	:	FColoredMaterialRenderProxy(InParent,InColor)
	{
	}
	virtual ~FDynamicColoredMaterialRenderProxy()
	{
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
	}
	virtual void ReleasePrimitiveResource()
	{
		delete this;
	}
};

#include "EdMode.h"

/*------------------------------------------------------------------------------
	Default.
------------------------------------------------------------------------------*/

/**
 * The default editing mode.  User can work with BSP and the builder brush. Vector and array properties are also visually editable.
 */
class FEdModeDefault : public FEdMode
{
public:
	FEdModeDefault();

	// FEdMode interface
	virtual bool UsesPropertyWidgets() const override { return true; }
	// End of FEdMode interface
};


#include "EditorModeManager.h"

