// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "EditorUndoClient.h"

FEditorUndoClient::~FEditorUndoClient()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}
