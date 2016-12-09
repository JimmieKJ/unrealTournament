// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlateSettings.generated.h"

/**
* Settings that control Slate functionality
*/
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Slate"))
class SLATE_API USlateSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Whether or not to send the TextChanged event instead of TextCommitted for the virtual keyboard */
	UPROPERTY(config, EditAnywhere, Category="VirtualKeyboard")
	bool bVirtualKeyboardSendsTextChanged;

	/** Display the virtual keyboard when it receives non-mouse focus. */
	UPROPERTY(config, EditAnywhere, Category="VirtualKeyboard")
	bool bVirtualKeyboardDisplayOnFocus;
};
