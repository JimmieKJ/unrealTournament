// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "SuperSearchSettings.generated.h"

enum class ESearchEngine : uint8;

/**
 * 
 */
UCLASS(config=EditorSettings, meta=( DisplayName="Super Search" ))
class USuperSearchSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/**
	 * Set the search engine to use in the editor for looking up questions in the community.
	 */
	UPROPERTY(EditAnywhere, config, Category=General)
	ESearchEngine SearchEngine;

public:
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};
