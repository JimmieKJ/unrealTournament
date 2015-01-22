// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FWidgetBlueprintEditor;

/**
 * Handles all of the toolbar related construction for the widget blueprint editor.
 */
class FWidgetBlueprintEditorToolbar : public TSharedFromThis<FWidgetBlueprintEditorToolbar>
{

public:
	/** Constructor */
	FWidgetBlueprintEditorToolbar(TSharedPtr<FWidgetBlueprintEditor>& InWidgetEditor);
	
	/**
	 * Builds the modes toolbar for the widget blueprint editor.
	 */
	void AddWidgetBlueprintEditorModesToolbar(TSharedPtr<FExtender> Extender);

public:
	/**  */
	void FillWidgetBlueprintEditorModesToolbar(FToolBarBuilder& ToolbarBuilder);

	TWeakPtr<FWidgetBlueprintEditor> WidgetEditor;
};
