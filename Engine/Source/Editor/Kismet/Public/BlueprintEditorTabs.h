// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct KISMET_API FBlueprintEditorTabs
{
	// Tab identifiers
	static const FName DetailsID;
	static const FName DefaultEditorID;
	static const FName DebugID;
	static const FName PaletteID;
	static const FName CompilerResultsID;
	static const FName FindResultsID;
	static const FName ConstructionScriptEditorID;
	static const FName SCSViewportID;
	static const FName MyBlueprintID;
	static const FName UserDefinedStructureID;

	// Document tab identifiers
	static const FName GraphEditorID;
	static const FName TimelineEditorID;

private:
	FBlueprintEditorTabs() {}
};

