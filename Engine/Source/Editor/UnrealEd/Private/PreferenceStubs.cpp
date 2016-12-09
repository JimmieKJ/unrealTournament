// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Preferences/CascadeOptions.h"
#include "Preferences/CurveEdOptions.h"
#include "Preferences/MaterialEditorOptions.h"
#include "Preferences/PersonaOptions.h"
#include "Preferences/PhATSimOptions.h"

// @todo find a better place for all of this, preferably in the appropriate modules
// though this would require the classes to be relocated as well

//
// UCascadeOptions
// 
UCascadeOptions::UCascadeOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// UPhATSimOptions ////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
UPhATSimOptions::UPhATSimOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PhysicsBlend = 1.0f;
	bUpdateJointsFromAnimation = false;
	MaxFPS = -1;
	TimeDilation = 1.f;
}

UMaterialEditorOptions::UMaterialEditorOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


UCurveEdOptions::UCurveEdOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPersonaOptions::UPersonaOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultLocalAxesSelection(2)
	, DefaultBoneDrawSelection(1)
{
	ViewModeIndex = VMI_Lit;

	SectionTimingNodeColor = FLinearColor(0.0f, 1.0f, 0.0f);
	NotifyTimingNodeColor = FLinearColor(1.0f, 0.0f, 0.0f);
	BranchingPointTimingNodeColor = FLinearColor(0.5f, 1.0f, 1.0f);

	bAutoAlignFloorToMesh = true;
}

void UPersonaOptions::SetViewportBackgroundColor( const FLinearColor& InViewportBackgroundColor)
{
	ViewportBackgroundColor = InViewportBackgroundColor;
	SaveConfig();
}

void UPersonaOptions::SetShowGrid( bool bInShowGrid )
{
	bShowGrid = bInShowGrid;
	SaveConfig();
}

void UPersonaOptions::SetHighlightOrigin( bool bInHighlightOrigin )
{
	bHighlightOrigin = bInHighlightOrigin;
	SaveConfig();
}

void UPersonaOptions::SetGridSize( int32 InGridSize )
{
	GridSize = InGridSize;
	SaveConfig();
}

void UPersonaOptions::SetViewModeIndex( EViewModeIndex InViewModeIndex )
{
	ViewModeIndex = InViewModeIndex;
	SaveConfig();
}

void UPersonaOptions::SetShowFloor( bool bInShowFloor )
{
	bShowFloor = bInShowFloor;
	SaveConfig();
}

void UPersonaOptions::SetAutoAlignFloorToMesh(bool bInAutoAlignFloorToMesh)
{
	bAutoAlignFloorToMesh = bInAutoAlignFloorToMesh;
	SaveConfig();
}

void UPersonaOptions::SetShowSky( bool bInShowSky )
{
	bShowSky = bInShowSky;
	SaveConfig();
}

void UPersonaOptions::SetMuteAudio( bool bInMuteAudio )
{
	bMuteAudio = bInMuteAudio;
	SaveConfig();
}

void UPersonaOptions::SetViewFOV( float InViewFOV )
{
	ViewFOV = InViewFOV;
	SaveConfig();
}

void UPersonaOptions::SetDefaultLocalAxesSelection( uint32 InDefaultLocalAxesSelection )
{
	DefaultLocalAxesSelection = InDefaultLocalAxesSelection;
	SaveConfig();
}

void UPersonaOptions::SetDefaultBoneDrawSelection(uint32 InDefaultBoneDrawSelection)
{
	DefaultBoneDrawSelection = InDefaultBoneDrawSelection;
	SaveConfig();
}

void UPersonaOptions::SetShowMeshStats( int32 InShowMeshStats )
{
	ShowMeshStats = InShowMeshStats;
	SaveConfig();
}


void UPersonaOptions::SetSectionTimingNodeColor(const FLinearColor& InColor)
{
	SectionTimingNodeColor = InColor;
	SaveConfig();
}

void UPersonaOptions::SetNotifyTimingNodeColor(const FLinearColor& InColor)
{
	NotifyTimingNodeColor = InColor;
	SaveConfig();
}

void UPersonaOptions::SetBranchingPointTimingNodeColor(const FLinearColor& InColor)
{
	BranchingPointTimingNodeColor = InColor;
	SaveConfig();
}
