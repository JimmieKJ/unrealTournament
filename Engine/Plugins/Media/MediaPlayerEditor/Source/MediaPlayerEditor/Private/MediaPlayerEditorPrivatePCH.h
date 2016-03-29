// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Private dependencies
 *****************************************************************************/

#include "CoreUObject.h"
#include "AssetToolsModule.h"
#include "SlateBasics.h"
#include "Engine.h"
#include "SlateStyle.h"
#include "EditorStyle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailsView.h"
#include "IMediaStream.h"
#include "IMediaAudioTrack.h"
#include "IMediaCaptionTrack.h"
#include "IMediaVideoTrack.h"
#include "MediaPlayer.h"
#include "MediaSampleBuffer.h"
#include "MediaSoundWave.h"
#include "MediaTexture.h"
#include "ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SceneViewport.h"
#include "SlateTextures.h"
#include "TickableObjectRenderThread.h"
#include "WorkspaceMenuStructureModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "UnrealEd.h"


/* Private includes
 *****************************************************************************/

#include "MediaPlayerCustomization.h"
#include "MediaSoundWaveCustomization.h"
#include "MediaTextureCustomization.h"

#include "MediaPlayerFactory.h"
#include "MediaPlayerFactoryNew.h"
#include "MediaSoundWaveFactoryNew.h"
#include "MediaTextureFactoryNew.h"

#include "MediaPlayerActions.h"
#include "MediaSoundWaveActions.h"
#include "MediaTextureActions.h"

#include "MediaPlayerEditorCommands.h"
#include "MediaPlayerEditorStyle.h"
#include "MediaPlayerEditorTexture.h"
#include "MediaPlayerEditorViewport.h"
#include "MediaPlayerEditorToolkit.h"

#include "SMediaPlayerEditorDetails.h"
#include "SMediaPlayerEditorViewer.h"
