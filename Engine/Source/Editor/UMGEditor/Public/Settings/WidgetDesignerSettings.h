// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetDesignerSettings.generated.h"

/**
 * Implements the settings for the Widget Blueprint Designer.
 */
UCLASS(config=EditorPerProjectUserSettings)
class UMGEDITOR_API UWidgetDesignerSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** If enabled, actor positions will snap to the grid. */
	UPROPERTY(EditAnywhere, config, Category = GridSnapping, meta = (DisplayName = "Enable Grid Snapping"))
	uint32 GridSnapEnabled:1;

	UPROPERTY(config)
	int32 GridSnapSize;

	UPROPERTY(EditAnywhere, config, Category = Dragging)
	bool bLockToPanelOnDragByDefault;

	UPROPERTY(EditAnywhere, config, Category = Visuals)
	bool bShowOutlines;
};
