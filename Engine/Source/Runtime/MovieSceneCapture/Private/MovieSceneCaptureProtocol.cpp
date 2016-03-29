// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "IMovieSceneCaptureProtocol.h"
#include "SceneViewport.h"

FCaptureProtocolInitSettings FCaptureProtocolInitSettings::FromSlateViewport(TSharedRef<FSceneViewport> InSceneViewport, UObject* InProtocolSettings)
{
	FCaptureProtocolInitSettings Settings;
	Settings.SceneViewport = InSceneViewport;
	Settings.DesiredSize = InSceneViewport->GetSize();
	Settings.ProtocolSettings = InProtocolSettings;
	return Settings;
}