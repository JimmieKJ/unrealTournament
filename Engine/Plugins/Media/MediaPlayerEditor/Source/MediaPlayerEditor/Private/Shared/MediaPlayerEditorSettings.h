// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaPlayerEditorSettings.generated.h"


UCLASS(config=EditorPerProjectUserSettings)
class UMediaPlayerEditorSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** The name of the desired native media player to use for playback. */
	UPROPERTY(config, EditAnywhere, Category=Viewer)
	FName DesiredPlayerName;

	/** Whether the media texture should be scaled to maintain the video's aspect ratio. */
	UPROPERTY(config, EditAnywhere, Category=Viewer)
	bool ScaleVideoToFit;

	/** Whether to display caption/subtitle text. */
	UPROPERTY(config, EditAnywhere, Category=Viewer)
	bool ShowCaptions;
};
