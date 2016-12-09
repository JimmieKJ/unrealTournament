// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"

class AActor;
class FMenuBuilder;
class FTrackEditorThumbnailPool;
class UFactory;
class UMovieSceneCameraCutTrack;

/**
 * Tools for camera cut tracks.
 */
class FCameraCutTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FCameraCutTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FCameraCutTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer .
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface

	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void Tick(float DeltaTime) override;
	virtual const FSlateBrush* GetIconBrush() const override;

protected:

	/** Delegate for AnimatablePropertyChanged in AddKey */
	bool AddKeyInternal(float AutoKeyTime, const FGuid ObjectGuid);

	/** Find or create a camera cut track in the currently focused movie scene. */
	UMovieSceneCameraCutTrack* FindOrCreateCameraCutTrack();

	UFactory* GetAssetFactoryForNewCameraCut(UClass* SequenceClass);

private:

	/** Callback for determining whether the "Add Camera Cut" menu entry can execute. */
	bool HandleAddCameraCutTrackMenuEntryCanExecute() const;

	/** Callback for executing the "Add Camera Cut Track" menu entry. */
	void HandleAddCameraCutTrackMenuEntryExecute();

	/** Callback for generating the menu of the "Add Camera Cut" combo button. */
	TSharedRef<SWidget> HandleAddCameraCutComboButtonGetMenuContent();

	/** Callback for whether a camera is pickable. */
	bool IsCameraPickable( const AActor* const PickableActor );

	/** Callback for executing a menu entry in the "Add Camera Cut" combo button. */
	void HandleAddCameraCutComboButtonMenuEntryExecute(AActor* Camera);

	/** Delegate for camera button lock state */
	ECheckBoxState IsCameraLocked() const; 

	/** Delegate for locked camera button */
	void OnLockCameraClicked(ECheckBoxState CheckBoxState);

	/** Delegate for camera button lock tooltip */
	FText GetLockCameraToolTip() const; 

private:

	/** The Thumbnail pool which draws all the viewport thumbnails for the camera cut track. */
	TSharedPtr<FTrackEditorThumbnailPool> ThumbnailPool;
};
