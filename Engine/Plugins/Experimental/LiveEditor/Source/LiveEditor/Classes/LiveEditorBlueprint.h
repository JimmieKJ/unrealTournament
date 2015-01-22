// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LiveEditorTypes.h"

#include "LiveEditorBlueprint.generated.h"

UCLASS(Abstract, Blueprintable)
class ULiveEditorBlueprint : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnInit();

	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnShutdown();

	UFUNCTION(BlueprintImplementableEvent)
	virtual void Tick( float DeltaTime );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	void SaveCheckpoint( int32 CheckpointID, const TArray<FLiveEditorCheckpointData> &CheckpointData );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	void LoadCheckpoint( int32 CheckpointID, TArray<FLiveEditorCheckpointData> &CheckpointData );

public:
	void DoInit();

	virtual class UWorld* GetWorld() const override;

private:
	TMap< int32, TArray<FLiveEditorCheckpointData> > CheckpointMap;
};
