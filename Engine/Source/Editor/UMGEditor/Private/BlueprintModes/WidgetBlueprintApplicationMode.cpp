// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/Kismet/Public/BlueprintEditorTabs.h"
#include "Editor/Kismet/Public/SBlueprintEditorToolbar.h"
#include "Editor/Kismet/Public/BlueprintEditorModes.h"

#include "WidgetBlueprintEditor.h"
#include "WidgetBlueprintApplicationMode.h"
#include "WidgetBlueprintApplicationModes.h"

/////////////////////////////////////////////////////
// FWidgetBlueprintApplicationMode

FWidgetBlueprintApplicationMode::FWidgetBlueprintApplicationMode(TSharedPtr<FWidgetBlueprintEditor> InWidgetEditor, FName InModeName)
	: FBlueprintEditorApplicationMode(InWidgetEditor, InModeName, FWidgetBlueprintApplicationModes::GetLocalizedMode, false, false)
	, MyWidgetBlueprintEditor(InWidgetEditor)
{
}

UWidgetBlueprint* FWidgetBlueprintApplicationMode::GetBlueprint() const
{
	if ( FWidgetBlueprintEditor* Editor = MyWidgetBlueprintEditor.Pin().Get() )
	{
		return Editor->GetWidgetBlueprintObj();
	}
	else
	{
		return NULL;
	}
}

TSharedPtr<FWidgetBlueprintEditor> FWidgetBlueprintApplicationMode::GetBlueprintEditor() const
{
	return MyWidgetBlueprintEditor.Pin();
}