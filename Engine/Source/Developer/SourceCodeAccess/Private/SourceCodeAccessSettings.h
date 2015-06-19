// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SourceCodeAccessSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings)
class USourceCodeAccessSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The source code editor we prefer to use. */
	UPROPERTY(Config, EditAnywhere, Category="Source Code Editor", meta=(DisplayName="Source Code Editor"))
	FString PreferredAccessor;
};