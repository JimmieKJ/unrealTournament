// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FShotTrackThumbnail;
class FShotTrackThumbnailPool;
class IMenu;
class ISectionLayoutBuilder;
class UMovieSceneShotSection;


/**
 * Shot section, which paints and ticks the appropriate section.
 */
class FShotSequencerSection
	: public ISequencerSection
	, public TSharedFromThis<FShotSequencerSection>
{
public:

	/** Create and initialize a new instance. */
	FShotSequencerSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FShotTrackThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection);

	/** Virtual destructor. */
	virtual ~FShotSequencerSection();

public:

	/** Draws the passed in viewport thumbnail and copies it to the thumbnail's texture. */
	void DrawViewportThumbnail(TSharedPtr<FShotTrackThumbnail> ShotThumbnail);

	/** @return The sequencer widget owning the shot section. */
	TSharedRef<SWidget> GetSequencerWidget()
	{
		return SequencerPtr.Pin()->GetSequencerWidget();
	}

	/** Gets the thumbnail width. */
	uint32 GetThumbnailWidth() const;

	/** Gets the time range of what in the sequencer is visible. */
	TRange<float> GetVisibleTimeRange() const
	{
		return VisibleTimeRange;
	}

	/** Regenerates all viewports and thumbnails at the new size. */
	void RegenerateViewportThumbnails(const FIntPoint& Size);

public:

	// ISequencerSection interface

	virtual bool AreSectionsConnected() const override;
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual void GenerateSectionLayout(ISectionLayoutBuilder& LayoutBuilder) const override { }
	virtual TSharedRef<SWidget> GenerateSectionWidget() override;
	virtual FText GetDisplayName() const override;
	virtual FName GetSectionGripLeftBrushName() const override;
	virtual FName GetSectionGripRightBrushName() const override;
	virtual float GetSectionGripSize() const override;
	virtual float GetSectionHeight() const override;
	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	ACameraActor* UpdateCameraObject() const;

private:

	/** Callback for executing a "Set Camera" menu entry in the context menu. */
	void HandleSetCameraMenuEntryExecute(AActor* InCamera);

	/** Callback for getting the text of the track name text block. */
	FText HandleShotNameTextBlockText() const;

	/** Callback for when the text of the track name text block has changed. */
	void HandleShotNameTextBlockTextCommitted(const FText& NewShotName, ETextCommit::Type CommitType);

private:

	/** The section we are visualizing. */
	UMovieSceneShotSection* Section;

	/** The parent sequencer we are a part of. */
	TWeakPtr<ISequencer> SequencerPtr;

	/** The actual camera actor we are possessing. */
	TWeakObjectPtr<ACameraActor> Camera;

	/** The thumbnail pool that we are sending all of our thumbnails to. */
	TWeakPtr<FShotTrackThumbnailPool> ThumbnailPool;

	/** A list of all thumbnails this shot section has. */
	TArray<TSharedPtr<FShotTrackThumbnail>> Thumbnails;

	/** The width of our thumbnails. */
	uint32 ThumbnailWidth;

	/** The stored size of this section in the Slate geometry. */
	FIntPoint StoredSize;

	/** The stored start time, to query for invalidations. */
	float StoredStartTime;

	/** Cached Time Range of the visible parent section area. */
	TRange<float> VisibleTimeRange;
	
	/** An internal viewport scene we use to render the thumbnails with. */
	TSharedPtr<FSceneViewport> InternalViewportScene;

	/** An internal editor viewport client to render the thumbnails with. */
	TSharedPtr<FLevelEditorViewportClient> InternalViewportClient;
	
	/** Fade brush. */
	const FSlateBrush* WhiteBrush;

	/** Reference to owner of the current popup. */
	TWeakPtr<IMenu> NameEntryPopupMenu;
};
