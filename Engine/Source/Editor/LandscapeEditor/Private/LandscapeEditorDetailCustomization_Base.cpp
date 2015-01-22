// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditorCommands.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorDetails.h"
#include "LandscapeEditorDetailCustomizations.h"

#include "PropertyHandle.h"


FEdModeLandscape* FLandscapeEditorDetailCustomization_Base::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

bool FLandscapeEditorDetailCustomization_Base::IsToolActive(FName ToolName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && LandscapeEdMode->CurrentTool != NULL)
	{
		const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();
		return CurrentToolName == ToolName;
	}

	return false;
}

bool FLandscapeEditorDetailCustomization_Base::IsBrushSetActive(FName BrushSetName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentBrushSetIndex >= 0)
	{
		const FName CurrentBrushSetName = LandscapeEdMode->LandscapeBrushSets[LandscapeEdMode->CurrentBrushSetIndex].BrushSetName;
		return CurrentBrushSetName == BrushSetName;
	}

	return false;
}

TOptional<int32> FLandscapeEditorDetailCustomization_Base::OnGetValue(TSharedRef<IPropertyHandle> PropertyHandle)
{
	// Try getting as float, if that fails then get as int
	float FloatValue = 0.0f;
	int32 IntValue = 0.0f;
	if (PropertyHandle->GetValue(FloatValue) == FPropertyAccess::Success)
	{
		return TOptional<int32>(FloatValue);
	}
	else if (ensure(PropertyHandle->GetValue(IntValue) == FPropertyAccess::Success))
	{
		return TOptional<int32>(IntValue);
	}

	// Value couldn't be accessed. Return an unset value
	return TOptional<int32>();
}

void FLandscapeEditorDetailCustomization_Base::OnValueChanged(int32 NewValue, TSharedRef<IPropertyHandle> PropertyHandle)
{
	// Try setting as float, if that fails then set as int
	EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::InteractiveChange;
	if (PropertyHandle->SetValue(NewValue, Flags) != FPropertyAccess::Success)
	{
		ensure(PropertyHandle->SetValue(NewValue, Flags) == FPropertyAccess::Success);
	}
}

void FLandscapeEditorDetailCustomization_Base::OnValueCommitted(int32 NewValue, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> PropertyHandle)
{
	// Try setting as float, if that fails then set as int
	if (PropertyHandle->SetValue(NewValue) != FPropertyAccess::Success)
	{
		ensure(PropertyHandle->SetValue(NewValue) == FPropertyAccess::Success);
	}
}

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorStructCustomization_Base::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}
