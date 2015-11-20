// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "STrack.h"
#include "SAnimTrackPanel.h"

DECLARE_DELEGATE( FOnPreAnimUpdate )
DECLARE_DELEGATE( FOnPostAnimUpdate )
DECLARE_DELEGATE_OneParam( FOnAnimSegmentNodeClicked, int32 )
DECLARE_DELEGATE_OneParam( FOnAnimSegmentRemoved, int32 )

//////////////////////////////////////////////////////////////////////////
// SAnimSegmentsPanel

// UI Commands
class FAnimSegmentsPanelCommands : public TCommands < FAnimSegmentsPanelCommands >
{
public:
	FAnimSegmentsPanelCommands()
		: TCommands<FAnimSegmentsPanelCommands>("AnimMontagePanel", NSLOCTEXT("Contexts", "AnimMontagePanel", "Anim Montage Panel"), NAME_None, FEditorStyle::GetStyleSetName())
	{

	}

	TSharedPtr<FUICommandInfo> DeleteSegment;

	virtual void RegisterCommands() override;
};

// TODO: comment about segment panel
class SAnimSegmentsPanel: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimSegmentsPanel )
		: _AnimTrack(NULL)
		, _ViewInputMin()
		, _ViewInputMax()
	{}

	SLATE_ARGUMENT( struct FAnimTrack*, AnimTrack)
	SLATE_ARGUMENT( STrackNodeSelectionSet *, NodeSelectionSet )
	SLATE_ARGUMENT( TSharedPtr<FTrackColorTracker>, ColorTracker)

	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	
	SLATE_ATTRIBUTE( FLinearColor,		NodeColor )
	SLATE_ATTRIBUTE( TArray<float>,		DraggableBars)
	SLATE_ATTRIBUTE( TArray<float>,		DraggableBarSnapPositions)
	SLATE_ATTRIBUTE( float,				ScrubPosition)
	SLATE_ATTRIBUTE( float,				TrackMaxValue)
	SLATE_ATTRIBUTE( int32,				TrackNumDiscreteValues)

	SLATE_EVENT( FOnAnimSegmentNodeClicked, OnAnimSegmentNodeClicked )
	SLATE_EVENT( FOnPreAnimUpdate,			OnPreAnimUpdate )
	SLATE_EVENT( FOnPostAnimUpdate,			OnPostAnimUpdate )
	SLATE_EVENT( FOnAnimSegmentRemoved,		OnAnimSegmentRemoved )

	SLATE_EVENT( FOnBarDrag,				OnBarDrag)
	SLATE_EVENT( FOnBarDrop,				OnBarDrop)
	SLATE_EVENT( FOnBarClicked,				OnBarClicked)

	SLATE_EVENT( FOnTrackRightClickContextMenu,	OnTrackRightClickContextMenu)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// We support keyboard focus to recieve UI key events
	bool SupportsKeyboardFocus() const override {return true;}

	// Key handler
	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:

	FOnPreAnimUpdate			OnPreAnimUpdateDelegate;
	FOnPostAnimUpdate			OnPostAnimUpdateDelegate;
	FOnAnimSegmentNodeClicked	OnAnimSegmentNodeClickedDelegate;
	FOnAnimSegmentRemoved		OnAnimSegmentRemovedDelegate;

	enum ETrackViewStyle
	{
		TRACK_Single,
		TRACK_Double,
		TRACK_ExpandAll
	};

	bool				ValidIndex(int32 AnimSegmentIndex) const;
	float				GetSegmentLength(int32 AnimSegmentIndex) const;
	float				GetSegmentStartPos(int32 AnimSegmentIndex) const;
	FString				GetAnimSegmentName(int32 AnimSegmentIndex) const;
	FText				GetAnimSegmentDetailedInfo(int32 AnimSegmentIndex) const;

	void				SetSegmentStartPos(float NewStartPos, int32 AnimSegmentIndex);
	void				OnSegmentDropped(int32 AnimSegmentIndex);

	void				ShowSegmentInDetailsView(int32 AnimSlotIndex, int32 AnimSegmentIndex);

	void				SummonSegmentNodeContextMenu( FMenuBuilder& MenuBuilder, int32 AnimSegmentIndex );

	void				AddAnimSegment(UAnimSequence *NewSequence, float NewStartPos );
	
	bool				DoesAnimTypeMatchTrack(UAnimSequence* NewSequence);

	void				OnTrackDragDrop( TSharedPtr<FDragDropOperation> DragDropOp, float DataPos );
	void				OnAnimSegmentNodeClicked(int32 SegmentIdx);

	// Remove all selected anim segments in all segment tracks
	void RemoveSelectedAnimSegments();

	// Remove the specified anim segment
	void RemoveAnimSegment(int32 AnimSegmentIndex);

	/** Bind UI commands for this widget */
	void				BindCommands();

	ETrackViewStyle		TrackStyle;

	TAttribute<float>	ViewInputMin;
	TAttribute<float>	ViewInputMax;

	struct FAnimTrack*	AnimTrack;

	bool bDragging;

	/** List of UI commands for this widget */
	TSharedPtr<FUICommandList> UICommandList;

	/** List of widgets representing tracks */
	TArray<TSharedPtr<STrack>> TrackWidgets;
};