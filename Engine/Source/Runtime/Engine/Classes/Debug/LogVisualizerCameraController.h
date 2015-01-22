// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DebugCameraController.h"
#include "LogVisualizerCameraController.generated.h"

UCLASS(config=Input, hidedropdown)
class ALogVisualizerCameraController : public ADebugCameraController
{
public:
	DECLARE_DELEGATE_OneParam(FActorSelectedDelegate, class AActor*);
	DECLARE_DELEGATE_OneParam(FLogEntryIterationDelegate, int32);

	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class AActor* PickedActor;

	virtual void PostInitializeComponents() override;

	ENGINE_API static ALogVisualizerCameraController* EnableCamera(UWorld* InWorld);
	ENGINE_API static void DisableCamera(UWorld* InWorld);
	ENGINE_API static bool IsEnabled(UWorld* InWorld);

	virtual void Select( FHitResult const& Hit ) override;

	void ShowNextEntry();
	void ShowPrevEntry();

	static TWeakObjectPtr<ALogVisualizerCameraController> Instance;

	FActorSelectedDelegate OnActorSelected;
	FLogEntryIterationDelegate OnIterateLogEntries;

protected:
	virtual void SetupInputComponent() override;
};
