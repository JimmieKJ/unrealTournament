// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FShotTrackThumbnailPool;
class UMovieSceneShotTrack;



/**
 * Tools for director tracks.
 */
class FShotTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FShotTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FShotTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer .
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface

	virtual void AddKey(const FGuid& ObjectGuid, UObject* AdditionalAsset = nullptr) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void Tick(float DeltaTime) override;

protected:

	/** Delegate for AnimatablePropertyChanged in AddKey */
	bool AddKeyInternal(float AutoKeyTime, const FGuid ObjectGuid);

	/** Find or create a shot track in the currently focused movie scene. */
	UMovieSceneShotTrack* FindOrCreateShotTrack();

	/** Finds the index in the ShotSections array where a new shot should be inserted */
	int32 FindIndexForNewShot(const TArray<UMovieSceneSection*>& ShotSections, float NewShotTime) const;

	/** Generates a shot number for a new section by looking at the existing shots in relation to the new shot time */
	int32 GenerateShotNumber(UMovieSceneShotTrack& ShotTrack, int32 SectionIndex) const;

	UFactory* GetAssetFactoryForNewShot(UClass* SequenceClass);

private:

	/** Callback for determining whether the "Add Shot Track" menu entry can execute. */
	bool HandleAddShotTrackMenuEntryCanExecute() const;

	/** Callback for executing the "Add Shot Track" menu entry. */
	void HandleAddShotTrackMenuEntryExecute();

	/** Callback for generating the menu of the "Add Shot" combo button. */
	TSharedRef<SWidget> HandleAddShotComboButtonGetMenuContent();

	/** Callback for executing a menu entry in the "Add Shot" combo button. */
	void HandleAddShotComboButtonMenuEntryExecute(AActor* Camera);

	/** Delegate for camera button lock state */
	ECheckBoxState IsCameraLocked() const; 

	/** Delegate for locked camera button */
	void OnLockCameraClicked(ECheckBoxState CheckBoxState);

	/** Delegate for camera button lock tooltip */
	FText GetLockCameraToolTip() const; 

private:

	/** The Thumbnail pool which draws all the viewport thumbnails for the director track. */
	TSharedPtr<FShotTrackThumbnailPool> ThumbnailPool;
};
