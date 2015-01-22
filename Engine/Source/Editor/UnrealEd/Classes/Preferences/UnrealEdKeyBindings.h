// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * This class handles hotkey binding management for the editor.
 */

#pragma once
#include "UnrealEdKeyBindings.generated.h"

/** An editor hotkey binding to a parameterless exec. */
USTRUCT()
struct FEditorKeyBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint32 bCtrlDown:1;

	UPROPERTY()
	uint32 bAltDown:1;

	UPROPERTY()
	uint32 bShiftDown:1;

	UPROPERTY()
	FKey Key;

	UPROPERTY()
	FName CommandName;

	FEditorKeyBinding()
		: bCtrlDown(false)
		, bAltDown(false)
		, bShiftDown(false)
	{}
};

UCLASS(Config=EditorKeyBindings)
class UUnrealEdKeyBindings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Array of keybindings */
	UPROPERTY(config)
	TArray<struct FEditorKeyBinding> KeyBindings;

};

