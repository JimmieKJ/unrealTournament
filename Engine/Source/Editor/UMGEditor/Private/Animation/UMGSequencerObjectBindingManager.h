// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerObjectBindingManager.h"

class FWidgetBlueprintEditor;
class UWidgetAnimation;

class FUMGSequencerObjectBindingManager : public ISequencerObjectBindingManager
{
public:
	FUMGSequencerObjectBindingManager( FWidgetBlueprintEditor& InWidgetBlueprintEditor, UWidgetAnimation& InAnimation );
	~FUMGSequencerObjectBindingManager();

	/** ISequencerObjectBindingManager interface */
	virtual bool AllowsSpawnableObjects() const override { return false; }
	virtual FGuid FindGuidForObject( const UMovieScene& MovieScene, UObject& Object ) const override;
	virtual void SpawnOrDestroyObjectsForInstance( TSharedRef<FMovieSceneInstance> MovieSceneInstance, bool bDestroyAll ) {}
	virtual void DestroyAllSpawnedObjects()  {}
	virtual bool CanPossessObject( UObject& Object ) const override;
	virtual void BindPossessableObject( const FGuid& PossessableGuid, UObject& PossessedObject ) override;
	virtual void UnbindPossessableObjects( const FGuid& PossessableGuid ) override;
	virtual void GetRuntimeObjects( const TSharedRef<FMovieSceneInstance>& MovieSceneInstance, const FGuid& ObjectGuid, TArray<UObject*>& OutRuntimeObjects ) const override;
	
	/** @return true if the current animation is valid */
	bool HasValidWidgetAnimation() const;

	/** @return the current widget animation */
	UWidgetAnimation* GetWidgetAnimation() { return WidgetAnimation.Get(); }

	/** Rebuilds mappings to live preview objects */
	void InitPreviewObjects();

private:
	void OnWidgetPreviewUpdated();
private:
	/** Mapping of preview objects to sequencer guids */
	TMap< TWeakObjectPtr<UObject>, FGuid> PreviewObjectToGuidMap;

	TWeakObjectPtr<UWidgetAnimation> WidgetAnimation;

	FWidgetBlueprintEditor& WidgetBlueprintEditor;
};
