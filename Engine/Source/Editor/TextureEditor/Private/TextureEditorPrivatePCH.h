// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureEditor.h"


/* Private dependencies
 *****************************************************************************/

#include "CubemapUnwrapUtils.h"
#include "Factories.h"
#include "MouseDeltaTracker.h"
#include "NormalMapPreview.h"
#include "SceneViewport.h"
#include "SColorPicker.h"
#include "ScopedTransaction.h"
#include "Texture2DPreview.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"


/* Private includes
 *****************************************************************************/

const double MaxZoom = 16.0;
const double MinZoom = 0.01;
const double ZoomStep = 0.1;


#include "TextureEditorCommands.h"
#include "TextureEditorSettings.h"
#include "TextureEditorViewOptionsMenu.h"
#include "STextureEditorViewportToolbar.h"
#include "STextureEditorViewport.h"
#include "TextureEditorViewportClient.h"
#include "TextureEditorToolkit.h"
