// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "IMovieSceneCaptureProtocol.h"
#include "SceneViewport.h"
#include "FileManager.h"


bool IMovieSceneCaptureProtocol::CanWriteToFile(const TCHAR* InFilename, bool bOverwriteExisting) const
{
	return bOverwriteExisting || IFileManager::Get().FileSize(InFilename) == -1;
}

FCaptureProtocolInitSettings FCaptureProtocolInitSettings::FromSlateViewport(TSharedRef<FSceneViewport> InSceneViewport, UObject* InProtocolSettings)
{
	FCaptureProtocolInitSettings Settings;
	Settings.SceneViewport = InSceneViewport;
	Settings.DesiredSize = InSceneViewport->GetSize();
	Settings.ProtocolSettings = InProtocolSettings;
	return Settings;
}