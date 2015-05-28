// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * An drag operation that occurs on a section or key
 */
class FSequencerDragOperation
{
public:
	FSequencerDragOperation( FSequencer& InSequencer )
		: Sequencer(InSequencer)
	{
		Settings = GetDefault<USequencerSettings>();
	}

	virtual ~FSequencerDragOperation(){}
	void BeginTransaction( UMovieSceneSection& Section, const FText& TransactionDesc );
	void EndTransaction();

	/**
	 * Notification that drag is starting
	 *
	 * @param LocalMousePos	The current relative (to sequencer) mouse location
	 * @param SequencerNode	The sequencer node that this operation is being performed on
	 */
	virtual void OnBeginDrag(const FVector2D& LocalMousePos, TSharedPtr<FTrackNode> SequencerNode) = 0;

	/**
	 * Notification that drag is ending
	 *
	 * @param SequencerNode	The sequencer node that this operation is being performed on
	 */
	virtual void OnEndDrag(TSharedPtr<FTrackNode> SequencerNode) = 0;

	virtual FCursorReply GetCursor() const { return FCursorReply::Cursor( EMouseCursor::Default ); }

	/**
	 * Notification called when the mouse moves while dragging
	 *
	 * @param MouseEvent	The associated mouse event for dragging
	 * @param LocalMousePos	The current relative (to sequencer) mouse location
	 * @param TimeToPixel	Utility for converting between time units and pixels
	 * @param SequencerNode	The sequencer node that this operation is being performed on
	 */
	virtual void OnDrag( const FPointerEvent& MouseEvent, const FVector2D& LocalMousePos, const struct FTimeToPixel& TimeToPixel, TSharedPtr<FTrackNode> SequencerNode ) = 0;

protected:
	/** Gets the boundaries that this section can extend to without trampling on other shots */
	TRange<float> GetSectionBoundaries(UMovieSceneSection* Section, TSharedPtr<FTrackNode> SequencerNode) const;

	/** Gets all potential snap times for this drag operation */
	void GetSectionSnapTimes(TArray<float>& OutSnapTimes, UMovieSceneSection* Section, TSharedPtr<FTrackNode> SequencerNode, bool bIgnoreOurSectionCustomSnaps);
	
	/**
	 * Given a set of initial times, and a set of snap times,
	 * finds the best initial time to snap to the snap times
	 *
	 * @param InitialTimes			The times which we want to snap WITH
	 * @param SnapTimes				The times which we want to snap TO
	 * @param TimeToPixelConverter	A converter for matching times to pixel units
	 * @param OutInitialTime		Passes back out the initial time that was snapped with, if something was snapped
	 * @param OutSnapTime			Passes back the snap time that was snapped to, if something was snapped
	 * @return True if something was snapped to
	 */
	bool SnapToTimes(TArray<float> InitialTimes, const TArray<float>& SnapTimes, const FTimeToPixel& TimeToPixelConverter, float& OutInitialTime, float& OutSnapTime);

	/**
	 * Heavy overload of the previous function, assumes there is only one InitialTime
	 * Doesn't have to muck about with pointers because we know that the only InitialTime is the one we need
	 *
	 * @param InitialTime			The time which we want to snap WITH
	 * @param SnapTimes				The times which we want to snap TO
	 * @param TimeToPixelConverter	A converter for matching times to pixel units
	 * @return						The snap time that was snapped to, or potentially nothing if nothing snapped
	 */
	TOptional<float> SnapToTimes(float InitialTimes, const TArray<float>& SnapTimes, const FTimeToPixel& TimeToPixelConverter);

	/** The current sequencer settings */
	const USequencerSettings* Settings;

	FSequencer& Sequencer;
};



/**
 * An operation to resize a section by dragging its left or right edge
 */
class FResizeSection : public FSequencerDragOperation
{
public:
	FResizeSection( FSequencer& Sequencer, UMovieSceneSection& InSection, bool bInDraggingByEnd );

	/** FSequencerDragOperation interface */
	virtual void OnBeginDrag(const FVector2D& LocalMousePos, TSharedPtr<FTrackNode> SequencerNode) override;
	virtual void OnEndDrag(TSharedPtr<FTrackNode> SequencerNode) override;
	virtual void OnDrag( const FPointerEvent& MouseEvent, const FVector2D& LocalMousePos, const FTimeToPixel& TimeToPixelConverter, TSharedPtr<FTrackNode> SequencerNode ) override;
	virtual FCursorReply GetCursor() const override { return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight ); }
private:
	/** The section we are interacting with */
	TWeakObjectPtr<UMovieSceneSection> Section;
	/** true if dragging  the end of the section, false if dragging the start */
	bool bDraggingByEnd;
};

/**
 * Operation to move an entire section
 */
class FMoveSection : public FSequencerDragOperation
{
public:
	FMoveSection( FSequencer& Sequencer, UMovieSceneSection& InSection );

	/** FSequencerDragOperation interface */
	virtual void OnBeginDrag(const FVector2D& LocalMousePos, TSharedPtr<FTrackNode> SequencerNode) override;
	virtual void OnEndDrag(TSharedPtr<FTrackNode> SequencerNode) override;
	virtual void OnDrag( const FPointerEvent& MouseEvent, const FVector2D& LocalMousePos, const FTimeToPixel& TimeToPixelConverter, TSharedPtr<FTrackNode> SequencerNode ) override;
	virtual FCursorReply GetCursor() const override { return FCursorReply::Cursor( EMouseCursor::CardinalCross ); }

private:
	/** Determines if the section, when moved to this the track index by a delta time, would still be valid */
	bool IsSectionAtNewLocationValid(UMovieSceneSection* Section, int32 TrackIndex, float DeltaTime, TSharedPtr<FTrackNode> SequencerNode) const;

private:
	/** The section we are interacting with */
	TWeakObjectPtr<UMovieSceneSection> Section;
	/** Local mouse position when dragging the section */
	FVector2D DragOffset;
};

/**
 * Operation to move keys of a section
 */
class FMoveKeys : public FSequencerDragOperation
{
public:
	FMoveKeys( FSequencer& Sequencer,  const TSet<FSelectedKey>* InSelectedKeys, FSelectedKey& PressedKey )
		: FSequencerDragOperation(Sequencer)
		, SelectedKeys( InSelectedKeys )
		, DraggedKey( PressedKey )
	{}

	/** FSequencerDragOperation interface */
	virtual void OnBeginDrag(const FVector2D& LocalMousePos, TSharedPtr<FTrackNode> SequencerNode) override;
	virtual void OnEndDrag(TSharedPtr<FTrackNode> SequencerNode) override;
	virtual void OnDrag( const FPointerEvent& MouseEvent, const FVector2D& LocalMousePos, const FTimeToPixel& TimeToPixelConverter, TSharedPtr<FTrackNode> SequencerNode ) override;

protected:	
	/** Gets all potential snap times for this drag operation */
	void GetKeySnapTimes(TArray<float>& OutSnapTimes, TSharedPtr<FTrackNode> SequencerNode);

private:
	/** The selected keys being moved. */
	const TSet<FSelectedKey>* SelectedKeys;
	/** The exact key that we're dragging */
	FSelectedKey DraggedKey;
};
