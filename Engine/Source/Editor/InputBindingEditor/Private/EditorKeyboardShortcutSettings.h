// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "EditorKeyboardShortcutSettings.generated.h"

UCLASS(config=EditorKeyBindings, meta=(DisplayName="Keyboard Shortcuts"))
class UEditorKeyboardShortcutSettings : public UDeveloperSettings
{
	GENERATED_BODY()
};
