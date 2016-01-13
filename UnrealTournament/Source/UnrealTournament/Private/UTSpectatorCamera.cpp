// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTSpectatorCamera.h"

AUTSpectatorCamera::AUTSpectatorCamera(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> UTEditorCameraMesh(TEXT("/Game/RestrictedAssets/EditorAssets/SpectatorCamera/SM_UTSpectatorCamera"));
		GetCameraComponent()->SetCameraMesh(UTEditorCameraMesh.Object);
	}
#endif
}
