// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BlueprintEditorModes.h"

/////////////////////////////////////////////////////
// FWidgetBlueprintApplicationMode

class FWidgetBlueprintApplicationMode : public FBlueprintEditorApplicationMode
{
public:
	FWidgetBlueprintApplicationMode(TSharedPtr<class FWidgetBlueprintEditor> InWidgetEditor, FName InModeName);

	// FApplicationMode interface
	// End of FApplicationMode interface

protected:
	UWidgetBlueprint* GetBlueprint() const;
	
	TSharedPtr<class FWidgetBlueprintEditor> GetBlueprintEditor() const;

	TWeakPtr<class FWidgetBlueprintEditor> MyWidgetBlueprintEditor;

	// Set of spawnable tabs in the mode
	FWorkflowAllowedTabSet TabFactories;
};
