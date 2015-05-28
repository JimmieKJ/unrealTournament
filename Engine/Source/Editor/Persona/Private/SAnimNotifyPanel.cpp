// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SAnimNotifyPanel.h"
#include "ScopedTransaction.h"
#include "SCurveEditor.h"
#include "Editor/KismetWidgets/Public/SScrubWidget.h"
#include "AssetRegistryModule.h"
#include "Editor/UnrealEd/Public/AssetNotifications.h"
#include "AssetSelection.h"
#include "STextEntryPopup.h"
#include "SExpandableArea.h"
#include "Toolkits/AssetEditorManager.h"
#include "BlueprintActionDatabase.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"

// Track Panel drawing
const float NotificationTrackHeight = 20.0f;

// AnimNotify Drawing
const float NotifyHeightOffset = 0.f;
const float NotifyHeight = NotificationTrackHeight + 3.f;
const FVector2D ScrubHandleSize(8.f, NotifyHeight);
const FVector2D AlignmentMarkerSize(8.f, NotifyHeight);
const FVector2D TextBorderSize(1.f, 1.f);

#define LOCTEXT_NAMESPACE "AnimNotifyPanel"

DECLARE_DELEGATE_OneParam( FOnDeleteNotify, struct FAnimNotifyEvent*)
DECLARE_DELEGATE_RetVal_FourParams( FReply, FOnNotifyNodeDragStarted, TSharedRef<SAnimNotifyNode>, const FPointerEvent&, const FVector2D&, const bool)
DECLARE_DELEGATE_RetVal_FiveParams(FReply, FOnNotifyNodesDragStarted, TArray<TSharedPtr<SAnimNotifyNode>>, TSharedRef<SWidget>, const FVector2D&, const FVector2D&, const bool)
DECLARE_DELEGATE_RetVal( float, FOnGetDraggedNodePos )
DECLARE_DELEGATE_TwoParams( FPanTrackRequest, int32, FVector2D)
DECLARE_DELEGATE_FourParams(FPasteNotifies, SAnimNotifyTrack*, float, ENotifyPasteMode::Type, ENotifyPasteMultipleMode::Type)

class FNotifyDragDropOp;


//////////////////////////////////////////////////////////////////////////
// SAnimNotifyNode

class SAnimNotifyNode : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimNotifyNode )
		: _Sequence()
		, _AnimNotify()
		, _OnNodeDragStarted()
		, _OnUpdatePanel()
		, _PanTrackRequest()
		, _OnDeselectAllNotifies()
		, _ViewInputMin()
		, _ViewInputMax()
		, _MarkerBars()
	{
	}
	SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
	SLATE_ARGUMENT( FAnimNotifyEvent *, AnimNotify )
	SLATE_EVENT( FOnNotifyNodeDragStarted, OnNodeDragStarted )
	SLATE_EVENT( FOnUpdatePanel, OnUpdatePanel )
	SLATE_EVENT( FPanTrackRequest, PanTrackRequest )
	SLATE_EVENT( FDeselectAllNotifies, OnDeselectAllNotifies)
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( TArray<FTrackMarkerBar>, MarkerBars)
	SLATE_END_ARGS()

	void Construct(const FArguments& Declaration);

	// SWidget interface
	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	// End of SWidget interface

	// SNodePanel::SNode interface
	void UpdateSizeAndPosition(const FGeometry& AllottedGeometry);
	FVector2D GetWidgetPosition() const;
	FVector2D GetNotifyPosition() const;
	FVector2D GetNotifyPositionOffset() const;
	FVector2D GetSize() const;
	bool HitTest(const FGeometry& AllottedGeometry, FVector2D MouseLocalPose) const;

	// Extra hit testing to decide whether or not the duration handles were hit on a state node
	ENotifyStateHandleHit::Type DurationHandleHitTest(const FVector2D& CursorScreenPosition) const;

	UObject* GetObjectBeingDisplayed() const;
	// End of SNodePanel::SNode

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** Helpers to draw scrub handles and snap offsets */
	void DrawHandleOffset( const float& Offset, const float& HandleCentre, FSlateWindowElementList& OutDrawElements, int32 MarkerLayer, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect ) const;
	void DrawScrubHandle( float ScrubHandleCentre, FSlateWindowElementList& OutDrawElements, int32 ScrubHandleID, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect, FLinearColor NodeColour ) const;

	FLinearColor GetNotifyColor() const;
	FText GetNotifyText() const;
	
	/** The notify that is being visualized by this node */
	FAnimNotifyEvent* NotifyEvent;

	void DropCancelled();

	/** Returns the size of this notifies duration in screen space */
	float GetDurationSize() const { return NotifyDurationSizeX;}

	/** Sets the position the mouse was at when this node was last hit */
	void SetLastMouseDownPosition(const FVector2D& CursorPosition) {LastMouseDownPosition = CursorPosition;}

	/** The minimum possible duration that a notify state can have */
	static const float MinimumStateDuration;

	const FVector2D& GetScreenPosition() const
	{
		return ScreenPosition;
	}

	const float GetLastSnappedTime() const
	{
		return LastSnappedTime;
	}

	void ClearLastSnappedTime()
	{
		LastSnappedTime = -1.0f;
	}

	void SetLastSnappedTime(float NewSnapTime)
	{
		LastSnappedTime = NewSnapTime;
	}

private:
	FText GetNodeTooltip() const;

	/** Detects any overflow on the anim notify track and requests a track pan */
	float HandleOverflowPan( const FVector2D& ScreenCursorPos, float TrackScreenSpaceXPosition );

	/** Finds a snap position if possible for the provided scrub handle, if it is not possible, returns -1.0f */
	float GetScrubHandleSnapPosition(float NotifyLocalX, ENotifyStateHandleHit::Type HandleToCheck);

	/** The sequence that the AnimNotifyEvent for Notify lives in */
	UAnimSequenceBase* Sequence;
	FSlateFontInfo Font;

	TAttribute<float>			ViewInputMin;
	TAttribute<float>			ViewInputMax;
	FVector2D					CachedAllotedGeometrySize;
	FVector2D					ScreenPosition;
	float						LastSnappedTime;

	bool						bDrawTooltipToRight;
	bool						bBeingDragged;
	bool						bSelected;

	// Index for undo transactions for dragging, as a check to make sure it's active
	int32						DragMarkerTransactionIdx;

	/** The scrub handle currently being dragged, if any */
	ENotifyStateHandleHit::Type CurrentDragHandle;
	
	float						NotifyTimePositionX;
	float						NotifyDurationSizeX;
	float						NotifyScrubHandleCentre;
	
	float						WidgetX;
	FVector2D					WidgetSize;
	
	FVector2D					TextSize;
	float						LabelWidth;
	FVector2D					BranchingPointIconSize;

	/** Last position the user clicked in the widget */
	FVector2D					LastMouseDownPosition;

	/** Delegate that is called when the user initiates dragging */
	FOnNotifyNodeDragStarted	OnNodeDragStarted;

	/** Delegate to pan the track, needed if the markers are dragged out of the track */
	FPanTrackRequest			PanTrackRequest;

	/** Marker bars for snapping to when dragging the markers in a state notify node */
	TAttribute<TArray<FTrackMarkerBar>> MarkerBars;

	/** Delegate to deselect notifies and clear the details panel */
	FDeselectAllNotifies OnDeselectAllNotifies;

	friend class SAnimNotifyTrack;
};

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyTrack

class SAnimNotifyTrack : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimNotifyTrack )
		: _Persona()
		, _Sequence(NULL)
		, _ViewInputMin()
		, _ViewInputMax()
		, _TrackIndex()
		, _TrackColor(FLinearColor::White)
		, _OnSelectionChanged()
		, _OnUpdatePanel()
		, _OnGetNotifyBlueprintData()
		, _OnGetNotifyStateBlueprintData()
		, _OnGetNotifyNativeClasses()
		, _OnGetNotifyStateNativeClasses()
		, _OnGetScrubValue()
		, _OnGetDraggedNodePos()
		, _OnNodeDragStarted()
		, _OnRequestTrackPan()
		, _OnRequestOffsetRefresh()
		, _OnDeleteNotify()
		, _OnGetIsAnimNotifySelectionValidForReplacement()
		, _OnReplaceSelectedWithNotify()
		, _OnReplaceSelectedWithBlueprintNotify()
		, _OnDeselectAllNotifies()
		, _OnCopyNotifies()
		, _OnPasteNotifies()
		, _OnSetInputViewRange()
		{}

		SLATE_ARGUMENT( TSharedPtr<FPersona>,	Persona )
		SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
		SLATE_ARGUMENT( TArray<FAnimNotifyEvent *>, AnimNotifies )
		SLATE_ATTRIBUTE( float, ViewInputMin )
		SLATE_ATTRIBUTE( float, ViewInputMax )
		SLATE_ATTRIBUTE( TArray<FTrackMarkerBar>, MarkerBars)
		SLATE_ARGUMENT( int32, TrackIndex )
		SLATE_ARGUMENT( FLinearColor, TrackColor )
		SLATE_EVENT(FOnTrackSelectionChanged, OnSelectionChanged)
		SLATE_EVENT( FOnUpdatePanel, OnUpdatePanel )
		SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyBlueprintData )
		SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyStateBlueprintData )
		SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyNativeClasses )
		SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyStateNativeClasses )
		SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
		SLATE_EVENT( FOnGetDraggedNodePos, OnGetDraggedNodePos )
		SLATE_EVENT( FOnNotifyNodesDragStarted, OnNodeDragStarted )
		SLATE_EVENT( FPanTrackRequest, OnRequestTrackPan )
		SLATE_EVENT( FRefreshOffsetsRequest, OnRequestOffsetRefresh )
		SLATE_EVENT( FDeleteNotify, OnDeleteNotify )
		SLATE_EVENT( FOnGetIsAnimNotifySelectionValidForReplacement, OnGetIsAnimNotifySelectionValidForReplacement)
		SLATE_EVENT( FReplaceWithNotify, OnReplaceSelectedWithNotify )
		SLATE_EVENT( FReplaceWithBlueprintNotify, OnReplaceSelectedWithBlueprintNotify)
		SLATE_EVENT( FDeselectAllNotifies, OnDeselectAllNotifies)
		SLATE_EVENT( FCopyNotifies, OnCopyNotifies )
		SLATE_EVENT( FPasteNotifies, OnPasteNotifies )
		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
		SLATE_END_ARGS()
public:

	/** Type used for list widget of tracks */
	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override { UpdateCachedGeometry( AllottedGeometry ); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	// End of SWidget interface

	/**
	 * Update the nodes to match the data that the panel is observing
	 */
	void Update();

	/** Returns the cached rendering geometry of this track */
	const FGeometry& GetCachedGeometry() const { return CachedGeometry; }

	FTrackScaleInfo GetCachedScaleInfo() const { return FTrackScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, CachedGeometry.Size); }

	/** Updates sequences when a notify node has been successfully dragged to a new position
	 *	@param Offset - Offset from the widget to the time handle 
	 */
	void HandleNodeDrop(TSharedPtr<SAnimNotifyNode> Node, float Offset = 0.0f);

	// Number of nodes in the track currently selected
	int32 GetNumSelectedNodes() const { return SelectedNotifyIndices.Num(); }

	// Index of the track in the notify panel
	int32 GetTrackIndex() const { return TrackIndex; }

	// Time at the position of the last mouseclick
	float GetLastClickedTime() const { return LastClickedTime; }

	// Removes the node widgets from the track and adds them to the provided Array
	void DisconnectSelectedNodesForDrag(TArray<TSharedPtr<SAnimNotifyNode>>& DragNodes);

	// Adds our current selection to the provided set
	void AppendSelectionToSet(FGraphPanelSelectionSet& SelectionSet);
	// Adds our current selection to the provided array
	void AppendSelectionToArray(TArray<FAnimNotifyEvent*>& Selection) const;
	// Gets the currently selected SAnimNotifyNode instances
	void AppendSelectedNodeWidgetsToArray(TArray<TSharedPtr<SAnimNotifyNode>>& NodeArray) const;
	// Gets the indices of the selected notifies
	TArray<int32> GetSelectedNotifyIndices() const {return SelectedNotifyIndices;}

	/**
	* Deselects all currently selected notify nodes
	* @param bUpdateSelectionSet - Whether we should report a selection change to the panel
	*/
	void DeselectAllNotifyNodes(bool bUpdateSelectionSet = true);

	// get Property Data of one element (NotifyIndex) from Notifies property of Sequence
	static uint8* FindNotifyPropertyData(UAnimSequenceBase* Sequence, int32 NotifyIndex, UArrayProperty*& ArrayProperty);

	// Paste a single Notify into this track from an exported string
	void PasteSingleNotify(FString& NotifyString, float PasteTime);

	// Uses the given track space rect and marquee information to refresh selection information
	void RefreshMarqueeSelectedNodes(FSlateRect& Rect, FNotifyMarqueeOperation& Marquee);

	// Create new notifies
	FAnimNotifyEvent& CreateNewBlueprintNotify(FString NewNotifyName, FString BlueprintPath, float StartTime);
	FAnimNotifyEvent& CreateNewNotify(FString NewNotifyName, UClass* NotifyClass, float StartTime);

	// Get the Blueprint Class from the path of the Blueprint
	static TSubclassOf<UObject> GetBlueprintClassFromPath(FString BlueprintPath);

	// Get the default Notify Name for a given blueprint notify asset
	FString MakeBlueprintNotifyName(FAssetData& NotifyAssetData);

protected:

	void CreateCommands();

	// Build up a "New Notify..." menu
	void FillNewNotifyMenu(FMenuBuilder& MenuBuilderbool, bool bIsReplaceWithMenu = false);
	void FillNewNotifyStateMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu  = false);

	// New notify functions
	void CreateNewBlueprintNotifyAtCursor(FString NewNotifyName, FString BlueprintPath);
	void CreateNewNotifyAtCursor(FString NewNotifyName, UClass* NotifyClass);
	void OnNewNotifyClicked();
	void AddNewNotify(const FText& NewNotifyName, ETextCommit::Type CommitInfo);

	// Trigger weight functions
	void OnSetTriggerWeightNotifyClicked(int32 NotifyIndex);
	void SetTriggerWeight(const FText& TriggerWeight, ETextCommit::Type CommitInfo, int32 NotifyIndex);

	// "Replace with... " commands
	void ReplaceSelectedWithBlueprintNotify(FString NewNotifyName, FString BlueprintPath);
	void ReplaceSelectedWithNotify(FString NewNotifyName, UClass* NotifyClass);
	void OnSetNotifyTimeClicked(int32 NotifyIndex);
	void SetNotifyTime(const FText& NotifyTimeText, ETextCommit::Type CommitInfo, int32 NotifyIndex);
	void OnSetNotifyFrameClicked(int32 NotifyIndex);
	void SetNotifyFrame(const FText& NotifyFrameText, ETextCommit::Type CommitInfo, int32 NotifyIndex);

	// Whether we have one node selected
	bool IsSingleNodeSelected();
	// Checks the clipboard for an anim notify buffer, and returns whether there's only one notify
	bool IsSingleNodeInClipboard();

	void OnSetDurationNotifyClicked(int32 NotifyIndex);
	void SetDuration(const FText& DurationText, ETextCommit::Type CommitInfo, int32 NotifyIndex);

	/** Function to copy anim notify event */
	void OnCopyNotifyClicked(int32 NotifyIndex);

	/** Function to check whether it is possible to paste anim notify event */
	bool CanPasteAnimNotify() const;

	/** Reads the contents of the clipboard for a notify to paste.
	 *  If successful OutBuffer points to the start of the notify object's data */
	bool ReadNotifyPasteHeader(FString& OutPropertyString, const TCHAR*& OutBuffer, float& OutOriginalTime, float& OutOriginalLength) const;

	/** Handler for context menu paste command */
	void OnPasteNotifyClicked(ENotifyPasteMode::Type PasteMode, ENotifyPasteMultipleMode::Type MultiplePasteType = ENotifyPasteMultipleMode::Absolute);

	/** Handler for popup window asking the user for a paste time */
	void OnPasteNotifyTimeSet(const FText& TimeText, ETextCommit::Type CommitInfo);

	/** Function to paste a previously copied notify */
	void OnPasteNotify(float TimeToPasteAt, ENotifyPasteMultipleMode::Type MultiplePasteType = ENotifyPasteMultipleMode::Absolute);

	/** Provides direct access to the notify menu from the context menu */
	void OnManageNotifies();

	/** Opens the supplied blueprint in an editor */
	void OnOpenNotifySource(UBlueprint* InSourceBlueprint) const;

	/**
	 * Selects a notify node in the graph. Supports multi selection
	 * @param NotifyIndex - Index of the notify node to select.
	 * @param Append - Whether to append to to current selection or start a new one.
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void SelectNotifyNode(int32 NotifyIndex, bool Append, bool bUpdateSelection = true);
	
	/**
	 * Toggles the selection status of a notify node, for example when
	 * Control is held when clicking.
	 * @param NotifyIndex - Index of the notify to toggle the selection status of
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void ToggleNotifyNodeSelectionStatus(int32 NotifyIndex, bool bUpdateSelection = true);

	/**
	 * Deselects requested notify node
	 * @param NotifyIndex - Index of the notify node to deselect
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void DeselectNotifyNode(int32 NotifyIndex, bool bUpdateSelection = true);

	int32 GetHitNotifyNode(const FGeometry& MyGeometry, const FVector2D& Position);

	TSharedPtr<SWidget> SummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	float CalculateTime( const FGeometry& MyGeometry, FVector2D NodePos, bool bInputIsAbsolute = true );

	// Handler that is called when the user starts dragging a node
	FReply OnNotifyNodeDragStarted( TSharedRef<SAnimNotifyNode> NotifyNode, const FPointerEvent& MouseEvent, const FVector2D& ScreenNodePosition, const bool bDragOnMarker, int32 NotifyIndex );

private:

	// Data structure for bluprint notify context menu entries
	struct BlueprintNotifyMenuInfo
	{
		FString NotifyName;
		FString BlueprintPath;
	};

	// Format notify asset data into the information needed for menu display
	void GetNotifyMenuData(TArray<FAssetData>& NotifyAssetData, TArray<BlueprintNotifyMenuInfo>& OutNotifyMenuData);

	// Store the tracks geometry for later use
	void UpdateCachedGeometry(const FGeometry& InGeometry) {CachedGeometry = InGeometry;}

	// Returns the padding needed to render the notify in the correct track position
	FMargin GetNotifyTrackPadding(int32 NotifyIndex) const
	{
		float LeftMargin = NotifyNodes[NotifyIndex]->GetWidgetPosition().X;
		float RightMargin = CachedGeometry.Size.X - LeftMargin - NotifyNodes[NotifyIndex]->GetSize().X;
		return FMargin(LeftMargin, 0, RightMargin, 0);
	}

	// Builds a UObject selection set and calls the OnSelectionChanged delegate
	void SendSelectionChanged()
	{
		OnSelectionChanged.ExecuteIfBound();
	}

protected:
	TSharedPtr<FUICommandList> AnimSequenceEditorActions;

	float LastClickedTime;

	class UAnimSequenceBase*				Sequence; // need for menu generation of anim notifies - 
	TArray<TSharedPtr<SAnimNotifyNode>>		NotifyNodes;
	TArray<FAnimNotifyEvent*>				AnimNotifies;
	TAttribute<float>						ViewInputMin;
	TAttribute<float>						ViewInputMax;
	TAttribute<FLinearColor>				TrackColor;
	int32									TrackIndex;
	FOnTrackSelectionChanged				OnSelectionChanged;
	FOnUpdatePanel							OnUpdatePanel;
	FOnGetBlueprintNotifyData				OnGetNotifyBlueprintData;
	FOnGetBlueprintNotifyData				OnGetNotifyStateBlueprintData;
	FOnGetNativeNotifyClasses				OnGetNotifyNativeClasses;
	FOnGetNativeNotifyClasses				OnGetNotifyStateNativeClasses;
	FOnGetScrubValue						OnGetScrubValue;
	FOnGetDraggedNodePos					OnGetDraggedNodePos;
	FOnNotifyNodesDragStarted				OnNodeDragStarted;
	FPanTrackRequest						OnRequestTrackPan;
	FDeselectAllNotifies					OnDeselectAllNotifies;
	FCopyNotifies							OnCopyNotifies;
	FPasteNotifies							OnPasteNotifies;
	FOnSetInputViewRange					OnSetInputViewRange;

	/** Delegate to call when offsets should be refreshed in a montage */
	FRefreshOffsetsRequest					OnRequestRefreshOffsets;

	/** Delegate to call when deleting notifies */
	FDeleteNotify							OnDeleteNotify;

	/** Delegates to call when replacing notifies */
	FOnGetIsAnimNotifySelectionValidForReplacement OnGetIsAnimNotifySelectionValidforReplacement;
	FReplaceWithNotify						OnReplaceSelectedWithNotify;
	FReplaceWithBlueprintNotify				OnReplaceSelectedWithBlueprintNotify;

	TSharedPtr<SBorder>						TrackArea;
	TWeakPtr<FPersona>						PersonaPtr;

	/** Cache the SOverlay used to store all this tracks nodes */
	TSharedPtr<SOverlay> NodeSlots;

	/** Cached for drag drop handling code */
	FGeometry CachedGeometry;

	/** Attribute for accessing any marker positions we have to draw */
	TAttribute<TArray<FTrackMarkerBar>>		MarkerBars;

	/** Nodes that are currently selected */
	TArray<int32> SelectedNotifyIndices;
};

//////////////////////////////////////////////////////////////////////////
// 

/** Widget for drawing a single track */
class SNotifyEdTrack : public SCompoundWidget
{
private:
	/** Index of Track in Sequence **/
	int32									TrackIndex;

	/** Anim Sequence **/
	class UAnimSequenceBase*				Sequence;

	/** Pointer to notify panel for drawing*/
	TWeakPtr<SAnimNotifyPanel>			AnimPanelPtr;
public:
	SLATE_BEGIN_ARGS( SNotifyEdTrack )
		: _TrackIndex(INDEX_NONE)
		, _AnimNotifyPanel()
		, _Sequence()
		, _WidgetWidth()
		, _ViewInputMin()
		, _ViewInputMax()
		, _OnSelectionChanged()
		, _OnUpdatePanel()
		, _OnDeleteNotify()
		, _OnDeselectAllNotifies()
		, _OnCopyNotifies()
		, _OnSetInputViewRange()
	{}
	SLATE_ARGUMENT( int32, TrackIndex )
	SLATE_ARGUMENT( TSharedPtr<SAnimNotifyPanel>, AnimNotifyPanel)
	SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
	SLATE_ARGUMENT( float, WidgetWidth )
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( TArray<FTrackMarkerBar>, MarkerBars)
	SLATE_EVENT( FOnTrackSelectionChanged, OnSelectionChanged)
	SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
	SLATE_EVENT( FOnGetDraggedNodePos, OnGetDraggedNodePos )
	SLATE_EVENT( FOnUpdatePanel, OnUpdatePanel )
	SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyBlueprintData )
	SLATE_EVENT( FOnGetBlueprintNotifyData, OnGetNotifyStateBlueprintData )
	SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyNativeClasses )
	SLATE_EVENT( FOnGetNativeNotifyClasses, OnGetNotifyStateNativeClasses )
	SLATE_EVENT( FOnNotifyNodesDragStarted, OnNodeDragStarted )
	SLATE_EVENT( FRefreshOffsetsRequest, OnRequestRefreshOffsets )
	SLATE_EVENT( FDeleteNotify, OnDeleteNotify )
	SLATE_EVENT( FDeselectAllNotifies, OnDeselectAllNotifies)
	SLATE_EVENT( FCopyNotifies, OnCopyNotifies )
	SLATE_EVENT( FPasteNotifies, OnPasteNotifies )
	SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	bool CanDeleteTrack();

	/** Pointer to actual anim notify track */
	TSharedPtr<class SAnimNotifyTrack>	NotifyTrack;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FNotifyDragDropOp : public FDragDropOperation
{
public:
	FNotifyDragDropOp(float& InCurrentDragXPosition) : 
		CurrentDragXPosition(InCurrentDragXPosition), 
		SnapTime(-1.f),
		SelectionTimeLength(0.0f)
	{
	}

	struct FTrackClampInfo
	{
		int32 TrackPos;
		int32 TrackSnapTestPos;
		float TrackMax;
		float TrackMin;
		float TrackWidth;
		TSharedPtr<SAnimNotifyTrack> NotifyTrack;
	};

	DRAG_DROP_OPERATOR_TYPE(FNotifyDragDropOp, FDragDropOperation)

	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override
	{
		if ( bDropWasHandled == false )
		{
			int32 NumNodes = SelectedNodes.Num();
			for(int32 CurrentNode = 0 ; CurrentNode < NumNodes ; ++CurrentNode)
			{
				TSharedPtr<SAnimNotifyNode> Node = SelectedNodes[CurrentNode];
				float NodePositionOffset = NodeXOffsets[CurrentNode];
				const FTrackClampInfo& ClampInfo = GetTrackClampInfo(Node->GetScreenPosition());
				ClampInfo.NotifyTrack->HandleNodeDrop(Node, NodePositionOffset);
				Node->DropCancelled();
			}

			Sequence->MarkPackageDirty();
			OnUpdatePanel.ExecuteIfBound();
		}
		
		FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
	}

	virtual void OnDragged( const class FDragDropEvent& DragDropEvent ) override
	{
		// Reset snapped node pointer
		SnappedNode = NULL;

		NodeGroupPosition = DragDropEvent.GetScreenSpacePosition() + DragOffset;

		FVector2D SelectionBeginPosition = NodeGroupPosition + SelectedNodes[0]->GetNotifyPositionOffset();
		
		FTrackClampInfo* SelectionPositionClampInfo = &GetTrackClampInfo(DragDropEvent.GetScreenSpacePosition());
		if(SelectionPositionClampInfo->NotifyTrack->GetTrackIndex() < TrackSpan)
		{
			// Our selection has moved off the bottom of the notify panel, adjust the clamping information to keep it on the panel
			SelectionPositionClampInfo = &ClampInfos[ClampInfos.Num() - TrackSpan - 1];
		}

		const FGeometry& TrackGeom = SelectionPositionClampInfo->NotifyTrack->GetCachedGeometry();
		const FTrackScaleInfo& TrackScaleInfo = SelectionPositionClampInfo->NotifyTrack->GetCachedScaleInfo();
		
		// Tracks the movement amount to apply to the selection due to a snap.
		float SnapMovement= 0.0f;
		// Clamp the selection into the track
		const float SelectionLocalLength = TrackScaleInfo.PixelsPerInput * SelectionTimeLength;
		const float ClampedEnd = FMath::Clamp(SelectionBeginPosition.X + NodeGroupSize.X, SelectionPositionClampInfo->TrackMin, SelectionPositionClampInfo->TrackMax);
		const float ClampedBegin = FMath::Clamp(SelectionBeginPosition.X, SelectionPositionClampInfo->TrackMin, SelectionPositionClampInfo->TrackMax);
		if(ClampedBegin > SelectionBeginPosition.X)
		{
			SelectionBeginPosition.X = ClampedBegin;
		}
		else if(ClampedEnd < SelectionBeginPosition.X + SelectionLocalLength)
		{
			SelectionBeginPosition.X = ClampedEnd - SelectionLocalLength;
		}

		// Handle node snaps
		for(int32 NodeIdx = 0 ; NodeIdx < SelectedNodes.Num() ; ++NodeIdx)
		{
			TSharedPtr<SAnimNotifyNode> CurrentNode = SelectedNodes[NodeIdx];
			FAnimNotifyEvent* CurrentEvent = CurrentNode->NotifyEvent;
			
			// Clear off any snap time currently stored
			CurrentNode->ClearLastSnappedTime();

			const FTrackClampInfo& NodeClamp = GetTrackClampInfo(CurrentNode->GetScreenPosition());

			FVector2D EventPosition = SelectionBeginPosition + FVector2D(TrackScaleInfo.PixelsPerInput * NodeTimeOffsets[NodeIdx], 0.0f);

			// Look for a snap on the first scrub handle
			FVector2D TrackNodePos = TrackGeom.AbsoluteToLocal(EventPosition);
			const FVector2D OriginalNodePosition = TrackNodePos;
			float SequenceEnd = TrackScaleInfo.InputToLocalX(Sequence->SequenceLength);

			float SnapX = GetSnapPosition(NodeClamp, TrackNodePos.X);
			if(SnapX >= 0.f)
			{
				EAnimEventTriggerOffsets::Type Offset = EAnimEventTriggerOffsets::NoOffset;
				if(SnapX == 0.0f || SnapX == SequenceEnd)
				{
					Offset = SnapX > 0.0f ? EAnimEventTriggerOffsets::OffsetBefore : EAnimEventTriggerOffsets::OffsetAfter;
				}
				else
				{
					Offset = (SnapX < TrackNodePos.X) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
				}

				CurrentEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
				CurrentNode->SetLastSnappedTime(TrackScaleInfo.LocalXToInput(SnapX));

				if(SnapMovement == 0.0f)
				{
					SnapMovement = SnapX - TrackNodePos.X;
					TrackNodePos.X = SnapX;
					SnapTime = TrackScaleInfo.LocalXToInput(SnapX);
					SnappedNode = CurrentNode;
				}
				EventPosition = NodeClamp.NotifyTrack->GetCachedGeometry().LocalToAbsolute(TrackNodePos);
			}
			else
			{
				CurrentEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
			}

			// Always clamp the Y to the current track
			SelectionBeginPosition.Y = SelectionPositionClampInfo->TrackPos;

			if(CurrentNode.IsValid() && CurrentEvent->GetDuration() > 0)
			{
				// If we didn't snap the beginning of the node, attempt to snap the end
				if(SnapX == -1.0f)
				{
					FVector2D TrackNodeEndPos = TrackNodePos + CurrentNode->GetDurationSize();
					SnapX = GetSnapPosition(*SelectionPositionClampInfo, TrackNodeEndPos.X);

					// Only attempt to snap if the node will fit on the track
					if(SnapX >= CurrentNode->GetDurationSize())
					{
						EAnimEventTriggerOffsets::Type Offset = EAnimEventTriggerOffsets::NoOffset;
						if(SnapX == SequenceEnd)
						{
							// Only need to check the end of the sequence here; end handle can't hit the beginning
							Offset = EAnimEventTriggerOffsets::OffsetBefore;
						}
						else
						{
							Offset = (SnapX < TrackNodeEndPos.X) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
						}
						CurrentEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
						
						if(SnapMovement == 0.0f)
						{
							SnapMovement = SnapX - TrackNodeEndPos.X;
							SnapTime = TrackScaleInfo.LocalXToInput(SnapX) - CurrentEvent->GetDuration();
							CurrentNode->SetLastSnappedTime(SnapTime);
							SnappedNode = CurrentNode;
						}
					}
					else
					{
						// Remove any trigger time if we can't fit the node in.
						CurrentEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
					}
				}
			}
		}

		SelectionBeginPosition.X += SnapMovement;

		CurrentDragXPosition = SelectionPositionClampInfo->NotifyTrack->GetCachedGeometry().AbsoluteToLocal(FVector2D(SelectionBeginPosition.X,0.0f)).X;

		CursorDecoratorWindow->MoveWindowTo(SelectionBeginPosition - SelectedNodes[0]->GetNotifyPositionOffset());
		NodeGroupPosition = SelectionBeginPosition;

		//scroll view
		float MouseXPos = DragDropEvent.GetScreenSpacePosition().X;
		if(MouseXPos < SelectionPositionClampInfo->TrackMin)
		{
			float ScreenDelta = MouseXPos - SelectionPositionClampInfo->TrackMin;
			RequestTrackPan.Execute(ScreenDelta, FVector2D(SelectionPositionClampInfo->TrackWidth, 1.f));
		}
		else if(MouseXPos > SelectionPositionClampInfo->TrackMax)
		{
			float ScreenDelta = MouseXPos - SelectionPositionClampInfo->TrackMax;
			RequestTrackPan.Execute(ScreenDelta, FVector2D(SelectionPositionClampInfo->TrackWidth, 1.f));
		}
	}

	float GetSnapPosition(const FTrackClampInfo& ClampInfo, float WidgetSpaceNotifyPosition)
	{
		const FTrackScaleInfo& ScaleInfo = ClampInfo.NotifyTrack->GetCachedScaleInfo();

		const float MaxSnapDist = 5.f;

		float CurrentMinSnapDest = MaxSnapDist;
		float SnapPosition = -1.f;

		if(MarkerBars.IsBound())
		{
			const TArray<FTrackMarkerBar>& Bars = MarkerBars.Get();
			if(Bars.Num() > 0)
			{
				for(int32 i = 0; i < Bars.Num(); ++i)
				{
					float WidgetSpaceSnapPosition = ScaleInfo.InputToLocalX(Bars[i].Time); // Do comparison in screen space so that zoom does not cause issues
					float ThisMinSnapDist = FMath::Abs(WidgetSpaceSnapPosition - WidgetSpaceNotifyPosition);
					if(ThisMinSnapDist < CurrentMinSnapDest)
					{
						CurrentMinSnapDest = ThisMinSnapDist;
						SnapPosition = WidgetSpaceSnapPosition;
					}
				}
			}
		}

		if(SnapPosition == -1.0f)
		{
			// Didn't snap to a bar, snap to the track bounds
			float WidgetSpaceEndPosition = ScaleInfo.InputToLocalX(Sequence->SequenceLength);
			float SnapDistBegin = FMath::Abs(-WidgetSpaceNotifyPosition);
			float SnapDistEnd = FMath::Abs(WidgetSpaceEndPosition - WidgetSpaceNotifyPosition);
			if(SnapDistBegin < CurrentMinSnapDest)
			{
				SnapPosition = 0.0f;
			}
			else if(SnapDistEnd < CurrentMinSnapDest)
			{
				SnapPosition = WidgetSpaceEndPosition;
			}
		}

		return SnapPosition;
	}

	FTrackClampInfo& GetTrackClampInfo(const FVector2D NodePos)
	{
		int32 ClampInfoIndex = 0;
		int32 SmallestNodeTrackDist = FMath::Abs(ClampInfos[0].TrackSnapTestPos - NodePos.Y);
		for(int32 i = 0; i < ClampInfos.Num(); ++i)
		{
			int32 Dist = FMath::Abs(ClampInfos[i].TrackSnapTestPos - NodePos.Y);
			if(Dist < SmallestNodeTrackDist)
			{
				SmallestNodeTrackDist = Dist;
				ClampInfoIndex = i;
			}
		}
		return ClampInfos[ClampInfoIndex];
	}

	class UAnimSequenceBase*			Sequence;				// The owning anim sequence
	FVector2D							DragOffset;				// Offset from the mouse to place the decorator
	TArray<FTrackClampInfo>				ClampInfos;				// Clamping information for all of the available tracks
	float&								CurrentDragXPosition;	// Current X position of the drag operation
	FPanTrackRequest					RequestTrackPan;		// Delegate to request a pan along the edges of a zoomed track
	TArray<float>						NodeTimes;				// Times to drop each selected node at
	float								SnapTime;				// The time that the snapped node was snapped to
	TWeakPtr<SAnimNotifyNode>			SnappedNode;			// The node chosen for the snap
	TAttribute<TArray<FTrackMarkerBar>>	MarkerBars;				// Branching point markers
	TArray<TSharedPtr<SAnimNotifyNode>> SelectedNodes;			// The nodes that are in the current selection
	TArray<float>						NodeTimeOffsets;		// Time offsets from the beginning of the selection to the nodes.
	TArray<float>						NodeXOffsets;			// Offsets in X from the widget position to the scrub handle for each node.
	FVector2D							NodeGroupPosition;		// Position of the beginning of the selection
	FVector2D							NodeGroupSize;			// Size of the entire selection
	TSharedPtr<SWidget>					Decorator;				// The widget to display when dragging
	float								SelectionTimeLength;	// Length of time that the selection covers
	int32								TrackSpan;				// Number of tracks that the selection spans
	FOnUpdatePanel						OnUpdatePanel;			// Delegate to redraw the notify panel

	static TSharedRef<FNotifyDragDropOp> New(
		TArray<TSharedPtr<SAnimNotifyNode>>			NotifyNodes, 
		TSharedPtr<SWidget>							Decorator, 
		const TArray<TSharedPtr<SAnimNotifyTrack>>& NotifyTracks, 
		class UAnimSequenceBase*					InSequence, 
		const FVector2D&							CursorPosition, 
		const FVector2D&							SelectionScreenPosition, 
		const FVector2D&							SelectionSize, 
		float&										CurrentDragXPosition, 
		FPanTrackRequest&							RequestTrackPanDelegate, 
		TAttribute<TArray<FTrackMarkerBar>>			MarkerBars,
		FOnUpdatePanel&								UpdatePanel
		)
	{
		TSharedRef<FNotifyDragDropOp> Operation = MakeShareable(new FNotifyDragDropOp(CurrentDragXPosition));
		Operation->Sequence = InSequence;
		Operation->RequestTrackPan = RequestTrackPanDelegate;
		Operation->OnUpdatePanel = UpdatePanel;

		Operation->NodeGroupPosition = SelectionScreenPosition;
		Operation->NodeGroupSize = SelectionSize;
		Operation->DragOffset = SelectionScreenPosition - CursorPosition;
		Operation->MarkerBars = MarkerBars;
		Operation->Decorator = Decorator;
		Operation->SelectedNodes = NotifyNodes;
		Operation->TrackSpan = NotifyNodes[0]->NotifyEvent->TrackIndex - NotifyNodes.Last()->NotifyEvent->TrackIndex;
		
		// Caclulate offsets for the selected nodes
		float BeginTime = MAX_flt;
		for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
		{
			float NotifyTime = Node->NotifyEvent->GetTime();

			if(NotifyTime < BeginTime)
			{
				BeginTime = NotifyTime;
			}
		}

		// Initialise node data
		for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
		{
			float NotifyTime = Node->NotifyEvent->GetTime();

			Node->ClearLastSnappedTime();
			Operation->NodeTimeOffsets.Add(NotifyTime - BeginTime);
			Operation->NodeTimes.Add(NotifyTime);
			Operation->NodeXOffsets.Add(Node->GetNotifyPositionOffset().X);

			// Calculate the time length of the selection. Because it is possible to have states
			// with arbitrary durations we need to search all of the nodes and find the furthest
			// possible point
			Operation->SelectionTimeLength = FMath::Max(Operation->SelectionTimeLength, NotifyTime + Node->NotifyEvent->GetDuration() - BeginTime);
		}

		Operation->Construct();

		for(int32 i = 0; i < NotifyTracks.Num(); ++i)
		{
			FTrackClampInfo Info;
			Info.NotifyTrack = NotifyTracks[i];
			const FGeometry& CachedGeometry = Info.NotifyTrack->GetCachedGeometry();
			Info.TrackPos = CachedGeometry.AbsolutePosition.Y;
			Info.TrackWidth = CachedGeometry.Size.X;
			Info.TrackMin = CachedGeometry.AbsolutePosition.X;
			Info.TrackMax = Info.TrackMin + Info.TrackWidth;
			Info.TrackSnapTestPos = Info.TrackPos + (CachedGeometry.Size.Y / 2);
			Operation->ClampInfos.Add(Info);
		}

		Operation->CursorDecoratorWindow->SetOpacity(0.5f);
		return Operation;
	}
	
	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return Decorator;
	}

	FText GetHoverText() const
	{
		FText HoverText = LOCTEXT("Invalid", "Invalid");

		if(SelectedNodes[0].IsValid())
		{
			HoverText = FText::FromName( SelectedNodes[0]->NotifyEvent->NotifyName );
		}

		return HoverText;
	}
};

//////////////////////////////////////////////////////////////////////////
// FAnimSequenceEditorCommands

class FAnimSequenceEditorCommands : public TCommands<FAnimSequenceEditorCommands>
{
public:
	FAnimSequenceEditorCommands()
		: TCommands<FAnimSequenceEditorCommands>(
		TEXT("AnimSequenceEditor"),
		NSLOCTEXT("Contexts", "AnimSequenceEditor", "Sequence Editor"),
		NAME_None, FEditorStyle::GetStyleSetName()
		)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND( SomeSequenceAction, "Some Sequence Action", "Does some sequence action", EUserInterfaceActionType::Button, FInputChord() );
	}
public:
	TSharedPtr<FUICommandInfo> SomeSequenceAction;
};

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyNode

const float SAnimNotifyNode::MinimumStateDuration = (1.0f / 30.0f);

void SAnimNotifyNode::Construct(const FArguments& InArgs)
{
	Sequence = InArgs._Sequence;
	NotifyEvent = InArgs._AnimNotify;
	Font = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );
	bBeingDragged = false;
	CurrentDragHandle = ENotifyStateHandleHit::None;
	bDrawTooltipToRight = true;
	bSelected = false;
	DragMarkerTransactionIdx = INDEX_NONE;
	
	// Cache notify name for blueprint / Native notifies.
	if(NotifyEvent->Notify)
	{
		NotifyEvent->NotifyName = FName(*NotifyEvent->Notify->GetNotifyName());
	}
	else if(NotifyEvent->NotifyStateClass)
	{
		NotifyEvent->NotifyName = FName(*NotifyEvent->NotifyStateClass->GetNotifyName());
	}

	OnNodeDragStarted = InArgs._OnNodeDragStarted;
	PanTrackRequest = InArgs._PanTrackRequest;
	OnDeselectAllNotifies = InArgs._OnDeselectAllNotifies;

	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;

	MarkerBars = InArgs._MarkerBars;

	SetToolTipText(TAttribute<FText>(this, &SAnimNotifyNode::GetNodeTooltip));
}

FReply SAnimNotifyNode::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FVector2D ScreenNodePosition = MyGeometry.AbsolutePosition;
	
	// Whether the drag has hit a duration marker
	bool bDragOnMarker = false;
	bBeingDragged = true;

	if(GetDurationSize() > 0.0f)
	{
		// This is a state node, check for a drag on the markers before movement. Use last screen space position before the drag started
		// as using the last position in the mouse event gives us a mouse position after the drag was started.
		ENotifyStateHandleHit::Type MarkerHit = DurationHandleHitTest(LastMouseDownPosition);
		if(MarkerHit == ENotifyStateHandleHit::Start || MarkerHit == ENotifyStateHandleHit::End)
		{
			bDragOnMarker = true;
			bBeingDragged = false;
			CurrentDragHandle = MarkerHit;

			// Modify the owning sequence as we're now dragging the marker and begin a transaction
			check(DragMarkerTransactionIdx == INDEX_NONE);
			DragMarkerTransactionIdx = GEditor->BeginTransaction(NSLOCTEXT("AnimNotifyNode", "StateNodeDragTransation", "Drag State Node Marker"));
			Sequence->Modify();
		}
	}

	return OnNodeDragStarted.Execute(SharedThis(this), MouseEvent, ScreenNodePosition, bDragOnMarker);
}

FLinearColor SAnimNotifyNode::GetNotifyColor() const
{
	FLinearColor BaseColor;

	if (NotifyEvent->Notify)
	{
		BaseColor = NotifyEvent->Notify->GetEditorColor();
	}
	else if (NotifyEvent->NotifyStateClass)
	{
		BaseColor = NotifyEvent->NotifyStateClass->GetEditorColor();
	}
	else
	{
		// Color for Custom notifies
		BaseColor = FLinearColor(1, 1, 0.5f);
	}

	BaseColor.A = 0.67f;

	return BaseColor;
}

FText SAnimNotifyNode::GetNotifyText() const
{
	// Combine comment from notify struct and from function on object
	return FText::FromName( NotifyEvent->NotifyName );
}

FText SAnimNotifyNode::GetNodeTooltip() const
{
	const float Time = NotifyEvent->GetTime();
	const FText Frame = FText::AsNumber(Sequence->GetFrameAtTime(NotifyEvent->GetTime()));
	const FText Seconds = FText::AsNumber( Time );

	FText ToolTipText;
	if (NotifyEvent->GetDuration() > 0.0f)
	{
		const FText Duration = FText::AsNumber( NotifyEvent->GetDuration() );
		ToolTipText = FText::Format( LOCTEXT("NodeToolTipLong", "@ {0} sec (frame {1}) for {2} sec"), Seconds, Frame, Duration );
	}
	else
	{
		ToolTipText = FText::Format(LOCTEXT("NodeToolTipShort", "@ {0} sec (frame {1})"), Seconds, Frame);
	}

	if (NotifyEvent->IsBranchingPoint())
	{
		ToolTipText = FText::Format(LOCTEXT("AnimNotify_ToolTipBranchingPoint", "{0} (BranchingPoint)"), ToolTipText);
	}
	return ToolTipText;
}

/** @return the Node's position within the graph */
UObject* SAnimNotifyNode::GetObjectBeingDisplayed() const
{
	if (NotifyEvent->Notify)
	{
		return NotifyEvent->Notify;
	}

	if (NotifyEvent->NotifyStateClass)
	{
		return NotifyEvent->NotifyStateClass;
	}

	return Sequence;
}

void SAnimNotifyNode::DropCancelled()
{
	bBeingDragged = false;
}

FVector2D SAnimNotifyNode::ComputeDesiredSize( float ) const
{
	return GetSize();
}

bool SAnimNotifyNode::HitTest(const FGeometry& AllottedGeometry, FVector2D MouseLocalPose) const
{
	FVector2D Position = GetWidgetPosition();
	FVector2D Size = GetSize();

	return MouseLocalPose >= Position && MouseLocalPose <= (Position + Size);
}

ENotifyStateHandleHit::Type SAnimNotifyNode::DurationHandleHitTest(const FVector2D& CursorTrackPosition) const
{
	ENotifyStateHandleHit::Type MarkerHit = ENotifyStateHandleHit::None;

	// Make sure this node has a duration box (meaning it is a state node)
	if(NotifyDurationSizeX > 0.0f)
	{
		// Test for mouse inside duration box with handles included
		float ScrubHandleHalfWidth = ScrubHandleSize.X / 2.0f;

		// Position and size of the notify node including the scrub handles
		FVector2D NotifyNodePosition(NotifyScrubHandleCentre - ScrubHandleHalfWidth, 0.0f);
		FVector2D NotifyNodeSize(NotifyDurationSizeX + ScrubHandleHalfWidth * 2.0f, NotifyHeight);

		FVector2D MouseRelativePosition(CursorTrackPosition - GetWidgetPosition());

		if(MouseRelativePosition > NotifyNodePosition && MouseRelativePosition < (NotifyNodePosition + NotifyNodeSize))
		{
			// Definitely inside the duration box, need to see which handle we hit if any
			if(MouseRelativePosition.X <= (NotifyNodePosition.X + ScrubHandleSize.X))
			{
				// Left Handle
				MarkerHit = ENotifyStateHandleHit::Start;
			}
			else if(MouseRelativePosition.X >= (NotifyNodePosition.X + NotifyNodeSize.X - ScrubHandleSize.X))
			{
				// Right Handle
				MarkerHit = ENotifyStateHandleHit::End;
			}
		}
	}

	return MarkerHit;
}

void SAnimNotifyNode::UpdateSizeAndPosition(const FGeometry& AllottedGeometry)
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, AllottedGeometry.Size);

	// Cache the geometry information, the alloted geometry is the same size as the track.
	CachedAllotedGeometrySize = AllottedGeometry.Size;

	NotifyTimePositionX = ScaleInfo.InputToLocalX(NotifyEvent->GetTime());
	NotifyDurationSizeX = ScaleInfo.PixelsPerInput * NotifyEvent->GetDuration();

	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	TextSize = FontMeasureService->Measure( GetNotifyText(), Font );
	LabelWidth = TextSize.X + (TextBorderSize.X * 2.f) + (ScrubHandleSize.X / 2.f);

	bool bDrawBranchingPoint = NotifyEvent->IsBranchingPoint();
	BranchingPointIconSize = FVector2D(TextSize.Y, TextSize.Y);
	if (bDrawBranchingPoint)
	{
		LabelWidth += BranchingPointIconSize.X + TextBorderSize.X * 2.f;
	}

	//Calculate scrub handle box size (the notional box around the scrub handle and the alignment marker)
	float NotifyHandleBoxWidth = FMath::Max(ScrubHandleSize.X, AlignmentMarkerSize.X * 2);

	// Work out where we will have to draw the tool tip
	FVector2D Size = GetSize();
	float LeftEdgeToNotify = NotifyTimePositionX;
	float RightEdgeToNotify = AllottedGeometry.Size.X - NotifyTimePositionX;
	bDrawTooltipToRight = (RightEdgeToNotify > LabelWidth) || (RightEdgeToNotify > LeftEdgeToNotify);

	// Calculate widget width/position based on where we are drawing the tool tip
	WidgetX = bDrawTooltipToRight ? (NotifyTimePositionX - (NotifyHandleBoxWidth / 2.f)) : (NotifyTimePositionX - LabelWidth);
	WidgetSize = bDrawTooltipToRight ? FVector2D(FMath::Max(LabelWidth, NotifyDurationSizeX), NotifyHeight) : FVector2D((LabelWidth + NotifyDurationSizeX), NotifyHeight);
	WidgetSize.X += NotifyHandleBoxWidth;

	// Widget position of the notify marker
	NotifyScrubHandleCentre = bDrawTooltipToRight ? NotifyHandleBoxWidth / 2.f : LabelWidth;
}

/** @return the Node's position within the track */
FVector2D SAnimNotifyNode::GetWidgetPosition() const
{
	return FVector2D(WidgetX, NotifyHeightOffset);
}

FVector2D SAnimNotifyNode::GetNotifyPosition() const
{
	return FVector2D(NotifyTimePositionX, NotifyHeightOffset);
}

FVector2D SAnimNotifyNode::GetNotifyPositionOffset() const
{
	return GetNotifyPosition() - GetWidgetPosition();
}

FVector2D SAnimNotifyNode::GetSize() const
{
	return WidgetSize;
}

int32 SAnimNotifyNode::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MarkerLayer = LayerId + 1;
	int32 ScrubHandleID = MarkerLayer + 1;
	int32 TextLayerID = ScrubHandleID + 1;
	int32 BranchPointLayerID = TextLayerID + 1;

	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush( TEXT("SpecialEditableTextImageNormal") );
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry(FVector2D(0,0), AllottedGeometry.Size), 
		StyleInfo, 
		MyClippingRect, 
		ESlateDrawEffect::None,
		FLinearColor(1.0f, 1.0f, 1.0f,0.1f));

	FText Text = GetNotifyText();
	FLinearColor NodeColour = bSelected ? FLinearColor(1.0f, 0.5f, 0.0f) : FLinearColor::Red;
	
	float HalfScrubHandleWidth = ScrubHandleSize.X / 2.0f;

	// Show duration of AnimNotifyState
	if( NotifyDurationSizeX > 0.f )
	{
		FLinearColor BoxColor = (NotifyEvent->TrackIndex % 2) == 0 ? FLinearColor(0.0f,1.0f,0.5f,0.5f) : FLinearColor(0.0f,0.5f,1.0f,0.5f);
		FVector2D DurationBoxSize = FVector2D(NotifyDurationSizeX, NotifyHeight);
		FVector2D DurationBoxPosition = FVector2D(NotifyScrubHandleCentre, 0.f);
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId, 
			AllottedGeometry.ToPaintGeometry(DurationBoxPosition, DurationBoxSize), 
			StyleInfo, 
			MyClippingRect, 
			ESlateDrawEffect::None,
			BoxColor);

		DrawScrubHandle(DurationBoxPosition.X + DurationBoxSize.X, OutDrawElements, ScrubHandleID, AllottedGeometry, MyClippingRect, NodeColour);
		
		// Render offsets if necessary
		if(NotifyEvent->EndTriggerTimeOffset != 0.f) //Do we have an offset to render?
		{
			float EndTime = NotifyEvent->GetTime() + NotifyEvent->GetDuration();
			if(EndTime != Sequence->SequenceLength) //Don't render offset when we are at the end of the sequence, doesnt help the user
			{
				// ScrubHandle
				float HandleCentre = NotifyDurationSizeX + ScrubHandleSize.X;
				DrawHandleOffset(NotifyEvent->EndTriggerTimeOffset, HandleCentre, OutDrawElements, MarkerLayer, AllottedGeometry, MyClippingRect);
			}
		}
	}

	// Branching point
	bool bDrawBranchingPoint = NotifyEvent->IsBranchingPoint();

	// Background
	FVector2D LabelSize = TextSize + TextBorderSize * 2.f;
	LabelSize.X += HalfScrubHandleWidth + (bDrawBranchingPoint ? (BranchingPointIconSize.X + TextBorderSize.X * 2.f) : 0.f);

	float LabelX = bDrawTooltipToRight ? NotifyScrubHandleCentre : NotifyScrubHandleCentre - LabelSize.X;
	float BoxHeight = (NotifyDurationSizeX > 0.f) ? (NotifyHeight - LabelSize.Y) : ((NotifyHeight - LabelSize.Y) / 2.f);

	FVector2D LabelPosition(LabelX, BoxHeight);

	FLinearColor NodeColor = SAnimNotifyNode::GetNotifyColor();

	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry(LabelPosition, LabelSize), 
		StyleInfo, 
		MyClippingRect, 
		ESlateDrawEffect::None,
		NodeColor);

	// Frame
	// Drawing lines is slow, reserved for single selected node
	if( bSelected )
	{
		TArray<FVector2D> LinePoints;

		LinePoints.Empty();
		LinePoints.Add(LabelPosition);
		LinePoints.Add(LabelPosition + FVector2D(LabelSize.X, 0.f));
		LinePoints.Add(LabelPosition + FVector2D(LabelSize.X, LabelSize.Y));
		LinePoints.Add(LabelPosition + FVector2D(0.f, LabelSize.Y));
		LinePoints.Add(LabelPosition);

		FSlateDrawElement::MakeLines( 
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			MyClippingRect,
			ESlateDrawEffect::None,
			FLinearColor::Black, 
			false
			);
	}

	// Text
	FVector2D TextPosition = LabelPosition + TextBorderSize;
	if(bDrawTooltipToRight)
	{
		TextPosition.X += HalfScrubHandleWidth;
	}
	TextPosition -= FVector2D(1.f, 1.f);

	FSlateDrawElement::MakeText( 
		OutDrawElements,
		TextLayerID,
		AllottedGeometry.ToPaintGeometry(TextPosition, TextSize),
		Text,
		Font,
		MyClippingRect,
		ESlateDrawEffect::None,
		FLinearColor::Black
		);

	DrawScrubHandle(NotifyScrubHandleCentre , OutDrawElements, ScrubHandleID, AllottedGeometry, MyClippingRect, NodeColour);

	if(NotifyEvent->TriggerTimeOffset != 0.f) //Do we have an offset to render?
	{
		float NotifyTime = NotifyEvent->GetTime();
		if(NotifyTime != 0.f && NotifyTime != Sequence->SequenceLength) //Don't render offset when we are at the start/end of the sequence, doesn't help the user
		{
			float HandleCentre = NotifyScrubHandleCentre;
			float &Offset = NotifyEvent->TriggerTimeOffset;
			
			DrawHandleOffset(NotifyEvent->TriggerTimeOffset, NotifyScrubHandleCentre, OutDrawElements, MarkerLayer, AllottedGeometry, MyClippingRect);
		}
	}

	// Draw Branching Point
	if (bDrawBranchingPoint)
	{
		FVector2D BranchPointIconPos = LabelPosition + LabelSize - BranchingPointIconSize - FVector2D(bDrawTooltipToRight ? TextBorderSize.X * 2.f : TextBorderSize.X * 4.f, 0.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			BranchPointLayerID,
			AllottedGeometry.ToPaintGeometry(BranchPointIconPos, BranchingPointIconSize),
			FEditorStyle::GetBrush(TEXT("AnimNotifyEditor.BranchingPoint")),
			MyClippingRect,
			ESlateDrawEffect::None,
			FLinearColor::White
			);
	}	

	return TextLayerID;
}

FReply SAnimNotifyNode::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Don't do scrub handle dragging if we haven't captured the mouse.
	if(!this->HasMouseCapture()) return FReply::Unhandled();

	// IF we get this far we should definitely have a handle to move.
	check(CurrentDragHandle != ENotifyStateHandleHit::None);
	
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, CachedAllotedGeometrySize);
	
	float TrackScreenSpaceXPosition = MyGeometry.AbsolutePosition.X - MyGeometry.Position.X;

	if(CurrentDragHandle == ENotifyStateHandleHit::Start)
	{
		// Check track bounds
		float OldDisplayTime = NotifyEvent->GetTime();

		if(MouseEvent.GetScreenSpacePosition().X >= TrackScreenSpaceXPosition && MouseEvent.GetScreenSpacePosition().X <= TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X)
		{
			float NewDisplayTime = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - MyGeometry.AbsolutePosition + MyGeometry.Position).X);
			float NewDuration = NotifyEvent->GetDuration() + OldDisplayTime - NewDisplayTime;

			// Check to make sure the duration is not less than the minimum allowed
			if(NewDuration < MinimumStateDuration)
			{
				NewDisplayTime -= MinimumStateDuration - NewDuration;
			}

			NotifyEvent->SetTime(NewDisplayTime);
			NotifyEvent->SetDuration(NotifyEvent->GetDuration() + OldDisplayTime - NotifyEvent->GetTime());
		}
		else if(NotifyEvent->GetDuration() > MinimumStateDuration)
		{
			float Overflow = HandleOverflowPan(MouseEvent.GetScreenSpacePosition(), TrackScreenSpaceXPosition);

			// Update scale info to the new view inputs after panning
			ScaleInfo.ViewMinInput = ViewInputMin.Get();
			ScaleInfo.ViewMaxInput = ViewInputMax.Get();

			NotifyEvent->SetTime(ScaleInfo.LocalXToInput(Overflow < 0.0f ? 0.0f : CachedAllotedGeometrySize.X));
			NotifyEvent->SetDuration(NotifyEvent->GetDuration() + OldDisplayTime - NotifyEvent->GetTime());

			// Adjust incase we went under the minimum
			if(NotifyEvent->GetDuration() < MinimumStateDuration)
			{
				float EndTimeBefore = NotifyEvent->GetTime() + NotifyEvent->GetDuration();
				NotifyEvent->SetTime(NotifyEvent->GetTime() + NotifyEvent->GetDuration() - MinimumStateDuration);
				NotifyEvent->SetDuration(MinimumStateDuration);
				float EndTimeAfter = NotifyEvent->GetTime() + NotifyEvent->GetDuration();
			}
		}

		// Now we know where the marker should be, look for possible snaps on montage marker bars
		float NodePositionX = ScaleInfo.InputToLocalX(NotifyEvent->GetTime());
		float MarkerSnap = GetScrubHandleSnapPosition(NodePositionX, ENotifyStateHandleHit::Start);
		if(MarkerSnap != -1.0f)
		{
			// We're near to a snap bar
			EAnimEventTriggerOffsets::Type Offset = (MarkerSnap < NodePositionX) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
			NotifyEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
			NodePositionX = MarkerSnap;

			// Adjust our start marker
			OldDisplayTime = NotifyEvent->GetTime();
			NotifyEvent->SetTime(ScaleInfo.LocalXToInput(NodePositionX));
			NotifyEvent->SetDuration(NotifyEvent->GetDuration() + OldDisplayTime - NotifyEvent->GetTime());
		}
		else
		{
			NotifyEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
		}
	}
	else
	{
		if(MouseEvent.GetScreenSpacePosition().X >= TrackScreenSpaceXPosition && MouseEvent.GetScreenSpacePosition().X <= TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X)
		{
			float NewDuration = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - MyGeometry.AbsolutePosition + MyGeometry.Position).X) - NotifyEvent->GetTime();

			NotifyEvent->SetDuration(FMath::Max(NewDuration, MinimumStateDuration));
		}
		else if(NotifyEvent->GetDuration() > MinimumStateDuration)
		{
			float Overflow = HandleOverflowPan(MouseEvent.GetScreenSpacePosition(), TrackScreenSpaceXPosition);

			// Update scale info to the new view inputs after panning
			ScaleInfo.ViewMinInput = ViewInputMin.Get();
			ScaleInfo.ViewMaxInput = ViewInputMax.Get();

			NotifyEvent->SetDuration(FMath::Max(ScaleInfo.LocalXToInput(Overflow > 0.0f ? CachedAllotedGeometrySize.X : 0.0f) - NotifyEvent->GetTime(), MinimumStateDuration));
		}

		// Now we know where the scrub handle should be, look for possible snaps on montage marker bars
		float NodePositionX = ScaleInfo.InputToLocalX(NotifyEvent->GetTime() + NotifyEvent->GetDuration());
		float MarkerSnap = GetScrubHandleSnapPosition(NodePositionX, ENotifyStateHandleHit::End);
		if(MarkerSnap != -1.0f)
		{
			// We're near to a snap bar
			EAnimEventTriggerOffsets::Type Offset = (MarkerSnap < NodePositionX) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
			NotifyEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
			NodePositionX = MarkerSnap;

			// Adjust our end marker
			NotifyEvent->SetDuration(ScaleInfo.LocalXToInput(NodePositionX) - NotifyEvent->GetTime());
		}
		else
		{
			NotifyEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
		}
	}

	return FReply::Handled();
}

FReply SAnimNotifyNode::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bLeftButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

	if(bLeftButton && CurrentDragHandle != ENotifyStateHandleHit::None)
	{
		// Clear the drag marker and give the mouse back
		CurrentDragHandle = ENotifyStateHandleHit::None;
		OnDeselectAllNotifies.ExecuteIfBound();

		// End drag transaction before handing mouse back
		check(DragMarkerTransactionIdx != INDEX_NONE);
		GEditor->EndTransaction();
		DragMarkerTransactionIdx = INDEX_NONE;

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

float SAnimNotifyNode::GetScrubHandleSnapPosition( float NotifyLocalX, ENotifyStateHandleHit::Type HandleToCheck )
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, CachedAllotedGeometrySize);

	const float MaxSnapDist = 5.0f;

	float CurrentMinSnapDistance = MaxSnapDist;
	float SnapPosition = -1.0f;
	float SnapTime = -1.0f;

	if(MarkerBars.IsBound())
	{
		const TArray<FTrackMarkerBar>& Bars = MarkerBars.Get();

		if(Bars.Num() > 0)
		{
			for(auto Iter(Bars.CreateConstIterator()) ; Iter ; ++Iter)
			{
				const FTrackMarkerBar& Bar = *Iter;
				float LocalSnapPosition = ScaleInfo.InputToLocalX(Bar.Time);
				float ThisNodeMinSnapDistance = FMath::Abs(LocalSnapPosition - NotifyLocalX);
				if(ThisNodeMinSnapDistance < CurrentMinSnapDistance)
				{
					CurrentMinSnapDistance = ThisNodeMinSnapDistance;
					SnapPosition = LocalSnapPosition;
				}
			}
		}
	}

	return SnapPosition;
}

float SAnimNotifyNode::HandleOverflowPan( const FVector2D &ScreenCursorPos, float TrackScreenSpaceXPosition )
{
	float Overflow = 0.0f;
	if(ScreenCursorPos.X < TrackScreenSpaceXPosition)
	{
		// Overflow left edge
		Overflow = ScreenCursorPos.X - TrackScreenSpaceXPosition;
	}
	else
	{
		// Overflow right edge
		Overflow = ScreenCursorPos.X - (TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X);
	}
	PanTrackRequest.ExecuteIfBound(Overflow, CachedAllotedGeometrySize);

	return Overflow;
}

void SAnimNotifyNode::DrawScrubHandle( float ScrubHandleCentre, FSlateWindowElementList& OutDrawElements, int32 ScrubHandleID, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect, FLinearColor NodeColour ) const
{
	FVector2D ScrubHandlePosition(ScrubHandleCentre - ScrubHandleSize.X / 2.0f, (NotifyHeight - ScrubHandleSize.Y) / 2.f);
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		ScrubHandleID, 
		AllottedGeometry.ToPaintGeometry(ScrubHandlePosition, ScrubHandleSize), 
		FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubHandleWhole" ) ), 
		MyClippingRect, 
		ESlateDrawEffect::None,
		NodeColour
		);
}

void SAnimNotifyNode::DrawHandleOffset( const float& Offset, const float& HandleCentre, FSlateWindowElementList& OutDrawElements, int32 MarkerLayer, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect ) const
{
	FVector2D MarkerPosition;
	FVector2D MarkerSize = AlignmentMarkerSize;

	if(Offset < 0.f)
	{
		MarkerPosition.Set( (HandleCentre) - AlignmentMarkerSize.X, (NotifyHeight - AlignmentMarkerSize.Y) / 2.f);
	}
	else
	{
		MarkerPosition.Set( HandleCentre + AlignmentMarkerSize.X, (NotifyHeight - AlignmentMarkerSize.Y) / 2.f);
		MarkerSize.X = -AlignmentMarkerSize.X;
	}

	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		MarkerLayer, 
		AllottedGeometry.ToPaintGeometry(MarkerPosition, MarkerSize), 
		FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.NotifyAlignmentMarker" ) ), 
		MyClippingRect, 
		ESlateDrawEffect::None,
		FLinearColor(0.f, 0.f, 1.f)
		);
}

void SAnimNotifyNode::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	ScreenPosition = AllottedGeometry.AbsolutePosition;
}

void SAnimNotifyNode::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if(CurrentDragHandle != ENotifyStateHandleHit::None)
	{
		// Lost focus while dragging a state node, clear the drag and end the current transaction
		CurrentDragHandle = ENotifyStateHandleHit::None;
		OnDeselectAllNotifies.ExecuteIfBound();
		
		check(DragMarkerTransactionIdx != INDEX_NONE);
		GEditor->EndTransaction();
		DragMarkerTransactionIdx = INDEX_NONE;
	}
}

bool SAnimNotifyNode::SupportsKeyboardFocus() const
{
	// Need to support focus on the node so we can end drag transactions if the user alt-tabs
	// from the editor while in the proceess of dragging a state notify duration marker.
	return true;
}

FCursorReply SAnimNotifyNode::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	// Show resize cursor if the cursor is hoverring over either of the scrub handles of a notify state node
	if(IsHovered() && GetDurationSize() > 0.0f)
	{
		FVector2D RelMouseLocation = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

		const float HandleHalfWidth = ScrubHandleSize.X / 2.0f;
		const float DistFromFirstHandle = FMath::Abs(RelMouseLocation.X - NotifyScrubHandleCentre);
		const float DistFromSecondHandle = FMath::Abs(RelMouseLocation.X - (NotifyScrubHandleCentre + NotifyDurationSizeX));

		if(DistFromFirstHandle < HandleHalfWidth || DistFromSecondHandle < HandleHalfWidth || CurrentDragHandle != ENotifyStateHandleHit::None)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		}
	}
	return FCursorReply::Unhandled();
}

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyTrack

void SAnimNotifyTrack::Construct(const FArguments& InArgs)
{
	FAnimSequenceEditorCommands::Register();
	CreateCommands();

	PersonaPtr = InArgs._Persona;
	Sequence = InArgs._Sequence;
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	AnimNotifies = InArgs._AnimNotifies;
	OnUpdatePanel = InArgs._OnUpdatePanel;
	OnGetNotifyBlueprintData = InArgs._OnGetNotifyBlueprintData;
	OnGetNotifyStateBlueprintData = InArgs._OnGetNotifyStateBlueprintData;
	OnGetNotifyNativeClasses = InArgs._OnGetNotifyNativeClasses;
	OnGetNotifyStateNativeClasses = InArgs._OnGetNotifyStateNativeClasses;
	TrackIndex = InArgs._TrackIndex;
	OnGetScrubValue = InArgs._OnGetScrubValue;
	OnGetDraggedNodePos = InArgs._OnGetDraggedNodePos;
	OnNodeDragStarted = InArgs._OnNodeDragStarted;
	TrackColor = InArgs._TrackColor;
	MarkerBars = InArgs._MarkerBars;
	OnRequestTrackPan = InArgs._OnRequestTrackPan;
	OnRequestRefreshOffsets = InArgs._OnRequestOffsetRefresh;
	OnDeleteNotify = InArgs._OnDeleteNotify;
	OnGetIsAnimNotifySelectionValidforReplacement = InArgs._OnGetIsAnimNotifySelectionValidForReplacement;
	OnReplaceSelectedWithNotify = InArgs._OnReplaceSelectedWithNotify;
	OnReplaceSelectedWithBlueprintNotify = InArgs._OnReplaceSelectedWithBlueprintNotify;
	OnDeselectAllNotifies = InArgs._OnDeselectAllNotifies;
	OnCopyNotifies = InArgs._OnCopyNotifies;
	OnPasteNotifies = InArgs._OnPasteNotifies;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;

	this->ChildSlot
	[
			SAssignNew( TrackArea, SBorder )
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.Padding( FMargin(0.f, 0.f) )
	];

	Update();
}

FVector2D SAnimNotifyTrack::ComputeDesiredSize( float ) const
{
	FVector2D Size;
	Size.X = 200;
	Size.Y = NotificationTrackHeight;
	return Size;
}

int32 SAnimNotifyTrack::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush( TEXT( "Persona.NotifyEditor.NotifyTrackBackground" ) );
	FLinearColor Color = TrackColor.Get();

	FPaintGeometry MyGeometry = AllottedGeometry.ToPaintGeometry();
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		MyGeometry, 
		StyleInfo, 
		MyClippingRect, 
		ESlateDrawEffect::None, 
		Color
		);

	int32 CustomLayerId = LayerId + 1; 

	// draw line for every 1/4 length
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, AllottedGeometry.Size);
	if (Sequence->GetNumberOfFrames() > 0 )
	{
		int32 Divider = SScrubWidget::GetDivider( ViewInputMin.Get(), ViewInputMax.Get(), AllottedGeometry.Size, Sequence->SequenceLength, Sequence->GetNumberOfFrames());

		float TimePerKey = Sequence->SequenceLength/(float)Sequence->GetNumberOfFrames();
		for (int32 I=1; I<Sequence->GetNumberOfFrames(); ++I)
		{
			if ( I % Divider == 0 )
			{
				float XPos = ScaleInfo.InputToLocalX(TimePerKey*I);

				TArray<FVector2D> LinePoints;
				LinePoints.Add(FVector2D(XPos, 0.f));
				LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));

				FSlateDrawElement::MakeLines( 
					OutDrawElements,
					CustomLayerId,
					MyGeometry,
					LinePoints,
					MyClippingRect,
					ESlateDrawEffect::None,
					FLinearColor::Black
					);
			}
		}
	}

	++CustomLayerId;
	for ( int32 I=0; I<NotifyNodes.Num(); ++I )
	{
		if ( NotifyNodes[I].Get()->bBeingDragged == false )
		{
			NotifyNodes[I].Get()->UpdateSizeAndPosition(AllottedGeometry);
		}
	}

	++CustomLayerId;

	float Value = 0.f;

	if ( OnGetScrubValue.IsBound() )
	{
		Value = OnGetScrubValue.Execute();
	}

	{
		float XPos = ScaleInfo.InputToLocalX(Value);

		TArray<FVector2D> LinePoints;
		LinePoints.Add(FVector2D(XPos, 0.f));
		LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));


		FSlateDrawElement::MakeLines( 
			OutDrawElements,
			CustomLayerId,
			MyGeometry,
			LinePoints,
			MyClippingRect,
			ESlateDrawEffect::None,
			FLinearColor::Red
			);
	}

	++CustomLayerId;

	if ( OnGetDraggedNodePos.IsBound() )
	{
		Value = OnGetDraggedNodePos.Execute();

		if(Value >= 0.0f)
		{
			float XPos = Value;
			TArray<FVector2D> LinePoints;
			LinePoints.Add(FVector2D(XPos, 0.f));
			LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));


			FSlateDrawElement::MakeLines( 
				OutDrawElements,
				CustomLayerId,
				MyGeometry,
				LinePoints,
				MyClippingRect,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 0.5f, 0.0f)
				);
		}
	}

	++CustomLayerId;

	// Draggable Bars
	for ( int32 I=0; MarkerBars.IsBound() && I < MarkerBars.Get().Num(); I++ )
	{
		// Draw lines
		FTrackMarkerBar Bar = MarkerBars.Get()[I];

		float XPos = ScaleInfo.InputToLocalX(Bar.Time);

		TArray<FVector2D> LinePoints;
		LinePoints.Add(FVector2D(XPos, 0.f));
		LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));

		FSlateDrawElement::MakeLines( 
			OutDrawElements,
			CustomLayerId,
			MyGeometry,
			LinePoints,
			MyClippingRect,
			ESlateDrawEffect::None,
			Bar.DrawColour
			);
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, CustomLayerId, InWidgetStyle, bParentEnabled);
}

FReply SAnimNotifyTrack::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float ZoomDelta = -0.1f * MouseEvent.GetWheelDelta();

	const FVector2D WidgetSpace = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float ZoomRatio = FMath::Clamp( (WidgetSpace.X / MyGeometry.Size.X), 0.f, 1.f);

	{
		const float InputViewSize = ViewInputMax.Get() - ViewInputMin.Get();
		const float InputChange = InputViewSize * ZoomDelta;

		float ViewMinInput = ViewInputMin.Get() - (InputChange * ZoomRatio);
		float ViewMaxInput = ViewInputMax.Get() + (InputChange * (1.f - ZoomRatio));

		OnSetInputViewRange.ExecuteIfBound(ViewMinInput, ViewMaxInput);
	}

	return FReply::Handled();
}

FCursorReply SAnimNotifyTrack::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ViewInputMin.Get() > 0.f || ViewInputMax.Get() < Sequence->SequenceLength)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}

	return FCursorReply::Unhandled();
}

// Fill "new notify state" menu, or "replace with notify state menu"
void SAnimNotifyTrack::FillNewNotifyStateMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu /* = false */)
{
	// Run the native query first to update the allowed classes for blueprints.
	TArray<UClass*> NotifyStateClasses;
	OnGetNotifyStateNativeClasses.ExecuteIfBound(NotifyStateClasses);

	// Collect blueprint notify data
	TArray<FAssetData> NotifyAssetData;
	TArray<BlueprintNotifyMenuInfo> NotifyMenuData;
	OnGetNotifyStateBlueprintData.ExecuteIfBound(NotifyAssetData);
	GetNotifyMenuData(NotifyAssetData, NotifyMenuData);

	for(BlueprintNotifyMenuInfo& NotifyData : NotifyMenuData)
	{
		const FText LabelText = FText::FromString(NotifyData.NotifyName);

		FUIAction UIAction;
		FText Description = FText::GetEmpty();
		if (!bIsReplaceWithMenu)
		{
			Description = LOCTEXT("AddsAnExistingAnimNotify", "Add an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::CreateNewBlueprintNotifyAtCursor,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
		}
		else
		{
			Description = LOCTEXT("ReplaceWithAnExistingAnimNotify", "Replace with an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::ReplaceSelectedWithBlueprintNotify,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
		}

		MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
	}

	MenuBuilder.BeginSection("NativeNotifyStates", LOCTEXT("NewStateNotifyMenu_Native", "Native Notify States"));
	{
		for(UClass* Class : NotifyStateClasses)
		{
			const FText Description = LOCTEXT("NewNotifyStateSubMenu_NativeToolTip", "Add an existing native notify state");
			FString Label = Class->GetDisplayNameText().ToString();
			const FText LabelText = FText::FromString(Label);

			FUIAction UIAction;
			if (!bIsReplaceWithMenu)
			{
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::CreateNewNotifyAtCursor,
					Label,
					Class);
			}
			else
			{
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::ReplaceSelectedWithNotify,
					Label,
					Class);
			}

			MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
		}
	}
	MenuBuilder.EndSection();
}

// Fill "new notify" menu, or "replace with notify menu"
void SAnimNotifyTrack::FillNewNotifyMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu /* = false */)
{
	TArray<UClass*> NativeNotifyClasses;
	OnGetNotifyNativeClasses.ExecuteIfBound(NativeNotifyClasses);

	TArray<FAssetData> NotifyAssetData;
	TArray<BlueprintNotifyMenuInfo> NotifyMenuData;
	OnGetNotifyBlueprintData.ExecuteIfBound(NotifyAssetData);
	GetNotifyMenuData(NotifyAssetData, NotifyMenuData);

	for(BlueprintNotifyMenuInfo& NotifyData : NotifyMenuData)
	{
		const FText LabelText = FText::FromString( NotifyData.NotifyName );

		FUIAction UIAction;
		FText Description = FText::GetEmpty();
		if (!bIsReplaceWithMenu)
		{
			Description = LOCTEXT("NewNotifySubMenu_ToolTip", "Add an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::CreateNewBlueprintNotifyAtCursor,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
		}
		else
		{
			Description = LOCTEXT("ReplaceWithNotifySubMenu_ToolTip", "Replace with an existing notify");
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::ReplaceSelectedWithBlueprintNotify,
				NotifyData.NotifyName,
				NotifyData.BlueprintPath);
		}
		
		MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
	}

	MenuBuilder.BeginSection("NativeNotifies", LOCTEXT("NewNotifyMenu_Native", "Native Notifies"));
	{
		for(UClass* Class : NativeNotifyClasses)
		{
			FString Label = Class->GetName();
			const FText LabelText = FText::FromString(Label);

			FUIAction UIAction;
			FText Description = FText::GetEmpty();
			if (!bIsReplaceWithMenu)
			{
				Description = LOCTEXT("NewNotifySubMenu_NativeToolTip", "Add an existing native notify");
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::CreateNewNotifyAtCursor,
					Label,
					Class);
			}
			else
			{
				Description = LOCTEXT("ReplaceWithNotifySubMenu_NativeToolTip", "Replace with an existing native notify");
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::ReplaceSelectedWithNotify,
					Label,
					Class);
			}

			MenuBuilder.AddMenuEntry(LabelText, Description, FSlateIcon(), UIAction);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimNotifyCustom", LOCTEXT("NewNotifySubMenu_Custom", "Custom"));
	{
		// now add custom anim notifiers
		USkeleton* SeqSkeleton = Sequence->GetSkeleton();
		if (SeqSkeleton)
		{
			for (int32 I = 0; I<SeqSkeleton->AnimationNotifies.Num(); ++I)
			{
				FName NotifyName = SeqSkeleton->AnimationNotifies[I];
				FString Label = NotifyName.ToString();

				FText Description = FText::GetEmpty();
				if (!bIsReplaceWithMenu)
				{
					Description = LOCTEXT("NewNotifySubMenu_ToolTip", "Add an existing notify");
				}
				else
				{
					Description = LOCTEXT("ReplaceWithNotifySubMenu_ToolTip", "Replace with an existing notify");
				}

				FUIAction UIAction;
				if (!bIsReplaceWithMenu)
				{
					UIAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::CreateNewNotifyAtCursor,
						Label,
						(UClass*)nullptr);
				}
				else
				{
					UIAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::ReplaceSelectedWithNotify,
						Label,
						(UClass*)nullptr);
				}

				MenuBuilder.AddMenuEntry( FText::FromString( Label ), Description, FSlateIcon(), UIAction);
			}
		}
	}
	MenuBuilder.EndSection();

	if (!bIsReplaceWithMenu)
	{
		MenuBuilder.BeginSection("AnimNotifyCreateNew");
		{
			FUIAction UIAction;
			UIAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::OnNewNotifyClicked);
			MenuBuilder.AddMenuEntry(LOCTEXT("NewNotify", "New Notify"), LOCTEXT("NewNotifyToolTip", "Create a new animation notify"), FSlateIcon(), UIAction);
		}
		MenuBuilder.EndSection();
	}
}

FAnimNotifyEvent& SAnimNotifyTrack::CreateNewBlueprintNotify(FString NewNotifyName, FString BlueprintPath, float StartTime)
{
	TSubclassOf<UObject> BlueprintClass = GetBlueprintClassFromPath(BlueprintPath);
	check(BlueprintClass);
	return CreateNewNotify(NewNotifyName, BlueprintClass, StartTime);
}

FAnimNotifyEvent& SAnimNotifyTrack::CreateNewNotify(FString NewNotifyName, UClass* NotifyClass, float StartTime)
{
	// Insert a new notify record and spawn the new notify object
	int32 NewNotifyIndex = Sequence->Notifies.Add(FAnimNotifyEvent());
	FAnimNotifyEvent& NewEvent = Sequence->Notifies[NewNotifyIndex];
	NewEvent.NotifyName = FName(*NewNotifyName);

	NewEvent.Link(Sequence, StartTime);
	NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(Sequence->CalculateOffsetForNotify(StartTime));
	NewEvent.TrackIndex = TrackIndex;

	if( NotifyClass )
	{
		class UObject* AnimNotifyClass = NewObject<UObject>(Sequence, NotifyClass, NAME_None, RF_Transactional);
		NewEvent.NotifyStateClass = Cast<UAnimNotifyState>(AnimNotifyClass);
		NewEvent.Notify = Cast<UAnimNotify>(AnimNotifyClass);

		// Set default duration to 1 frame for AnimNotifyState.
		if( NewEvent.NotifyStateClass )
		{
			NewEvent.NotifyName = FName(*NewEvent.NotifyStateClass->GetNotifyName());
			NewEvent.SetDuration(1 / 30.f);
			NewEvent.EndLink.Link(Sequence, NewEvent.EndLink.GetTime());
		}
		else
		{
			NewEvent.NotifyName = FName(*NewEvent.Notify->GetNotifyName());
		}
	}
	else
	{
		NewEvent.Notify = NULL;
		NewEvent.NotifyStateClass = NULL;
	}

	if(NewEvent.Notify)
	{
		TArray<FAssetData> SelectedAssets;
		AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

		for( TFieldIterator<UObjectProperty> PropIt(NewEvent.Notify->GetClass()); PropIt; ++PropIt )
		{
			if(PropIt->GetBoolMetaData(TEXT("ExposeOnSpawn")))
			{
				UObjectProperty* Property = *PropIt;
				const FAssetData* Asset = SelectedAssets.FindByPredicate([Property](const FAssetData& Other)
				{
					return Other.GetAsset()->IsA(Property->PropertyClass);
				});

				if( Asset )
				{
					uint8* Offset = (*PropIt)->ContainerPtrToValuePtr<uint8>(NewEvent.Notify);
					(*PropIt)->ImportText( *Asset->GetAsset()->GetPathName(), Offset, 0, NewEvent.Notify );
					break;
				}
			}
		}

		NewEvent.Notify->OnAnimNotifyCreatedInEditor(NewEvent);
	}
	else if (NewEvent.NotifyStateClass)
	{
		NewEvent.NotifyStateClass->OnAnimNotifyCreatedInEditor(NewEvent);
	}

	Sequence->MarkPackageDirty();

	return NewEvent;
}

void SAnimNotifyTrack::CreateNewBlueprintNotifyAtCursor(FString NewNotifyName, FString BlueprintPath)
{
	TSubclassOf<UObject> BlueprintClass = GetBlueprintClassFromPath(BlueprintPath);
	check(BlueprintClass);
	CreateNewNotifyAtCursor(NewNotifyName, BlueprintClass);
}

void SAnimNotifyTrack::CreateNewNotifyAtCursor(FString NewNotifyName, UClass* NotifyClass)
{
	const FScopedTransaction Transaction(LOCTEXT("AddNotifyEvent", "Add Anim Notify"));
	Sequence->Modify();
	CreateNewNotify(NewNotifyName, NotifyClass, LastClickedTime);
	OnUpdatePanel.ExecuteIfBound();
}

void SAnimNotifyTrack::ReplaceSelectedWithBlueprintNotify(FString NewNotifyName, FString BlueprintPath)
{
	OnReplaceSelectedWithBlueprintNotify.ExecuteIfBound(NewNotifyName, BlueprintPath);
}

void SAnimNotifyTrack::ReplaceSelectedWithNotify(FString NewNotifyName, UClass* NotifyClass)
{
	OnReplaceSelectedWithNotify.ExecuteIfBound(NewNotifyName, NotifyClass);
}


TSubclassOf<UObject> SAnimNotifyTrack::GetBlueprintClassFromPath(FString BlueprintPath)
{
	TSubclassOf<UObject> BlueprintClass = NULL;
	if (!BlueprintPath.IsEmpty())
	{
		UBlueprint* BlueprintLibPtr = LoadObject<UBlueprint>(NULL, *BlueprintPath, NULL, 0, NULL);
		BlueprintClass = Cast<UClass>(BlueprintLibPtr->GeneratedClass);
	}
	return BlueprintClass;
}

FReply SAnimNotifyTrack::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bLeftMouseButton =  MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	bool bRightMouseButton =  MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	bool bShift = MouseEvent.IsShiftDown();
	bool bCtrl = MouseEvent.IsControlDown();

	if ( bRightMouseButton )
	{
		TSharedPtr<SWidget> WidgetToFocus;
		WidgetToFocus = SummonContextMenu(MyGeometry, MouseEvent);

		return (WidgetToFocus.IsValid())
			? FReply::Handled().ReleaseMouseCapture().SetUserFocus(WidgetToFocus.ToSharedRef(), EFocusCause::SetDirectly)
			: FReply::Handled().ReleaseMouseCapture();
	}
	else if ( bLeftMouseButton )
	{
		FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
		CursorPos = MyGeometry.AbsoluteToLocal(CursorPos);
		int32 NotifyIndex = GetHitNotifyNode(MyGeometry, CursorPos);

		if(NotifyIndex == INDEX_NONE)
		{
			// Clicked in empty space, clear selection
			OnDeselectAllNotifies.ExecuteIfBound();
		}
		else
		{
			if(bCtrl)
			{
				ToggleNotifyNodeSelectionStatus(NotifyIndex);
			}
			else
			{
				SelectNotifyNode(NotifyIndex, bShift);
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAnimNotifyTrack::SelectNotifyNode(int32 NotifyIndex, bool Append, bool bUpdateSelection)
{
	if( NotifyIndex != INDEX_NONE )
	{
		// Deselect all other notifies if necessary.
		if (Sequence && !Append)
		{
			OnDeselectAllNotifies.ExecuteIfBound();
		}

		// Check to see if we've already selected this node
		if (!SelectedNotifyIndices.Contains(NotifyIndex))
		{
			// select new one
			if (NotifyNodes.IsValidIndex(NotifyIndex))
			{
				TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[NotifyIndex];
				Node->bSelected = true;
				SelectedNotifyIndices.Add(NotifyIndex);

				if(bUpdateSelection)
				{
					SendSelectionChanged();
				}
			}
		}
	}
}

void SAnimNotifyTrack::ToggleNotifyNodeSelectionStatus( int32 NotifyIndex, bool bUpdateSelection )
{
	check(NotifyNodes.IsValidIndex(NotifyIndex));

	bool bSelected = SelectedNotifyIndices.Contains(NotifyIndex);
	if(bSelected)
	{
		SelectedNotifyIndices.Remove(NotifyIndex);
	}
	else
	{
		SelectedNotifyIndices.Add(NotifyIndex);
	}

	TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[NotifyIndex];
	Node->bSelected = !Node->bSelected;

	if(bUpdateSelection)
	{
		SendSelectionChanged();
	}
}

void SAnimNotifyTrack::DeselectNotifyNode( int32 NotifyIndex, bool bUpdateSelection )
{
	check(NotifyNodes.IsValidIndex(NotifyIndex));
	TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[NotifyIndex];
	Node->bSelected = false;

	int32 ItemsRemoved = SelectedNotifyIndices.Remove(NotifyIndex);
	check(ItemsRemoved > 0);

	if(bUpdateSelection)
	{
		SendSelectionChanged();
	}
}

void SAnimNotifyTrack::DeselectAllNotifyNodes(bool bUpdateSelectionSet)
{
	for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
	{
		Node->bSelected = false;
	}
	SelectedNotifyIndices.Empty();

	if(bUpdateSelectionSet)
	{
		SendSelectionChanged();
	}
}

TSharedPtr<SWidget> SAnimNotifyTrack::SummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
	int32 NotifyIndex = GetHitNotifyNode(MyGeometry, MyGeometry.AbsoluteToLocal(CursorPos));
	LastClickedTime = CalculateTime(MyGeometry, MouseEvent.GetScreenSpacePosition());

	const bool bCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bCloseWindowAfterMenuSelection, AnimSequenceEditorActions );
	FUIAction NewAction;

	MenuBuilder.BeginSection("AnimNotify", LOCTEXT("NotifyHeading", "Notify") );
	{
		if ( NotifyIndex != INDEX_NONE )
		{
			if (!NotifyNodes[NotifyIndex]->bSelected)
			{
				SelectNotifyNode(NotifyIndex, MouseEvent.IsControlDown());
			}

			FAnimNotifyEvent* Event = AnimNotifies[NotifyIndex];

			FNumberFormattingOptions Options;
			Options.MinimumFractionalDigits = 5;

			// Add item to directly set notify time
			const FText CurrentTime = FText::AsNumber(Event->GetTime(), &Options);
			const FText TimeMenuText = FText::Format(LOCTEXT("TimeMenuText", "Set Notify Begin Time: {0}..."), CurrentTime);

			NewAction.ExecuteAction.BindRaw(this, &SAnimNotifyTrack::OnSetNotifyTimeClicked, NotifyIndex);
			NewAction.CanExecuteAction.BindRaw(this, &SAnimNotifyTrack::IsSingleNodeSelected);

			MenuBuilder.AddMenuEntry(TimeMenuText, LOCTEXT("SetTimeToolTip", "Set the time of this notify directly"), FSlateIcon(), NewAction);

			// Add item to directly set notify frame
			const FText Frame = FText::AsNumber(Sequence->GetFrameAtTime(Event->GetTime()));
			const FText FrameMenuText = FText::Format(LOCTEXT("FrameMenuText", "Set Notify Frame: {0}..."), Frame);

			NewAction.ExecuteAction.BindRaw(this, &SAnimNotifyTrack::OnSetNotifyFrameClicked, NotifyIndex);
			NewAction.CanExecuteAction.BindRaw(this, &SAnimNotifyTrack::IsSingleNodeSelected);

			MenuBuilder.AddMenuEntry(FrameMenuText, LOCTEXT("SetFrameToolTip", "Set the frame of this notify directly"), FSlateIcon(), NewAction);

			// add menu to get threshold weight for triggering this notify
			NewAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::OnSetTriggerWeightNotifyClicked, NotifyIndex);
			NewAction.CanExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::IsSingleNodeSelected);

			const FText Threshold = FText::AsNumber( AnimNotifies[NotifyIndex]->TriggerWeightThreshold, &Options );
			const FText MinTriggerWeightText = FText::Format( LOCTEXT("MinTriggerWeight", "Min Trigger Weight: {0}..."), Threshold );
			MenuBuilder.AddMenuEntry(MinTriggerWeightText, LOCTEXT("MinTriggerWeightToolTip", "The minimum weight to trigger this notify"), FSlateIcon(), NewAction);

			// Add menu for changing duration if this is an AnimNotifyState
			if( AnimNotifies[NotifyIndex]->NotifyStateClass )
			{
				NewAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::OnSetDurationNotifyClicked, NotifyIndex);
				NewAction.CanExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::IsSingleNodeSelected);

				FText SetAnimStateDurationText = FText::Format( LOCTEXT("SetAnimStateDuration", "Set AnimNotifyState duration ({0})"), FText::AsNumber( AnimNotifies[NotifyIndex]->GetDuration() ) );
				MenuBuilder.AddMenuEntry(SetAnimStateDurationText, LOCTEXT("SetAnimStateDuration_ToolTip", "The duration of this AnimNotifyState"), FSlateIcon(), NewAction);
			}

		}
		else
		{
			MenuBuilder.AddSubMenu(
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotify", "Add Notify..."),
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotifyToolTip", "Add AnimNotifyEvent"),
				FNewMenuDelegate::CreateRaw( this, &SAnimNotifyTrack::FillNewNotifyMenu, false ) );

			MenuBuilder.AddSubMenu(
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotifyState", "Add Notify State..."),
				NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuAddNotifyStateToolTip","Add AnimNotifyState"),
				FNewMenuDelegate::CreateRaw( this, &SAnimNotifyTrack::FillNewNotifyStateMenu, false ) );

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("NewNotifySubMenu", "ManageNotifies", "Manage Notifies..."),
				NSLOCTEXT("NewNotifySubMenu", "ManageNotifiesToolTip", "Opens the Manage Notifies window"),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP( this, &SAnimNotifyTrack::OnManageNotifies ) ) );
		}
	}
	MenuBuilder.EndSection(); //AnimNotify

	NewAction.CanExecuteAction = 0;

	MenuBuilder.BeginSection("AnimEdit", LOCTEXT("NotifyEditHeading", "Edit") );
	{
		if ( NotifyIndex != INDEX_NONE )
		{
			// copy notify menu item
			NewAction.ExecuteAction.BindRaw(
				this, &SAnimNotifyTrack::OnCopyNotifyClicked, NotifyIndex);
			MenuBuilder.AddMenuEntry(LOCTEXT("Copy", "Copy"), LOCTEXT("CopyToolTip", "Copy animation notify event"), FSlateIcon(), NewAction);

			// allow it to delete
			NewAction.ExecuteAction = OnDeleteNotify;
			MenuBuilder.AddMenuEntry(LOCTEXT("Delete", "Delete"), LOCTEXT("DeleteToolTip", "Delete animation notify"), FSlateIcon(), NewAction);

			FAnimNotifyEvent* Notify = AnimNotifies[NotifyIndex];

			// For the "Replace With..." menu, make sure the current AnimNotify selection is valid for replacement
			if (OnGetIsAnimNotifySelectionValidforReplacement.IsBound() && OnGetIsAnimNotifySelectionValidforReplacement.Execute())
			{
				// If this is an AnimNotifyState (has duration) allow it to be replaced with other AnimNotifyStates
				if (Notify && Notify->NotifyStateClass)
				{
					MenuBuilder.AddSubMenu(
						NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotifyState", "Replace with Notify State..."),
						NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotifyStateToolTip", "Replace with AnimNotifyState"),
						FNewMenuDelegate::CreateRaw(this, &SAnimNotifyTrack::FillNewNotifyStateMenu, true));
				}
				// If this is a regular AnimNotify (no duration) allow it to be replaced with other AnimNotifies
				else 
				{
					MenuBuilder.AddSubMenu(
						NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotify", "Replace with Notify..."),
						NSLOCTEXT("NewNotifySubMenu", "NewNotifySubMenuReplaceWithNotifyToolTip", "Replace with AnimNotifyEvent"),
						FNewMenuDelegate::CreateRaw(this, &SAnimNotifyTrack::FillNewNotifyMenu, true));
				}
			}
		}
		else
		{
			FString PropertyString;
			const TCHAR* Buffer;
			float OriginalTime;
			float OriginalLength;

			//Check whether can we show menu item to paste anim notify event
			if( ReadNotifyPasteHeader(PropertyString, Buffer, OriginalTime, OriginalLength) )
			{
				// paste notify menu item
				if (IsSingleNodeInClipboard())
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::MousePosition, ENotifyPasteMultipleMode::Absolute);

					MenuBuilder.AddMenuEntry(LOCTEXT("Paste", "Paste"), LOCTEXT("PasteToolTip", "Paste animation notify event here"), FSlateIcon(), NewAction);
				}
				else
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::MousePosition, ENotifyPasteMultipleMode::Relative);

					MenuBuilder.AddMenuEntry(LOCTEXT("PasteMultRel", "Paste Multiple Relative"), LOCTEXT("PasteToolTip", "Paste multiple notifies beginning at the mouse cursor, maintaining the same relative spacing as the source."), FSlateIcon(), NewAction);

					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::MousePosition, ENotifyPasteMultipleMode::Absolute);

					MenuBuilder.AddMenuEntry(LOCTEXT("PasteMultAbs", "Paste Multiple Absolute"), LOCTEXT("PasteToolTip", "Paste multiple notifies beginning at the mouse cursor, maintaining absolute spacing."), FSlateIcon(), NewAction);
				}

				if(OriginalTime < Sequence->SequenceLength)
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SAnimNotifyTrack::OnPasteNotifyClicked, ENotifyPasteMode::OriginalTime, ENotifyPasteMultipleMode::Absolute);

					FText DisplayText = FText::Format( LOCTEXT("PasteAtOriginalTime", "Paste at original time ({0})"), FText::AsNumber( OriginalTime) );
					MenuBuilder.AddMenuEntry(DisplayText, LOCTEXT("PasteAtOriginalTimeToolTip", "Paste animation notify event at the time it was set to when it was copied"), FSlateIcon(), NewAction);
				}
				
			}
		}
	}
	MenuBuilder.EndSection(); //AnimEdit

	if (NotifyIndex != INDEX_NONE)
	{
		FAnimNotifyEvent* Notify = AnimNotifies[NotifyIndex];
		UObject* NotifyObject = Notify->Notify;
		NotifyObject = NotifyObject ? NotifyObject : Notify->NotifyStateClass;

		if (NotifyObject && Cast<UBlueprintGeneratedClass>(NotifyObject->GetClass()))
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(NotifyObject->GetClass()->ClassGeneratedBy))
			{
				MenuBuilder.BeginSection("ViewSource", LOCTEXT("NotifyViewHeading", "View"));

				NewAction.ExecuteAction.BindRaw(
					this, &SAnimNotifyTrack::OnOpenNotifySource, Blueprint);
				MenuBuilder.AddMenuEntry(LOCTEXT("OpenNotifyBlueprint", "Open Notify Blueprint"), LOCTEXT("OpenNotifyBlueprint", "Opens the source blueprint for this notify"), FSlateIcon(), NewAction);

				MenuBuilder.EndSection(); //ViewSource
			}
		}
	}

	// Display the newly built menu
	TWeakPtr<SWindow> ContextMenuWindow =
		FSlateApplication::Get().PushMenu( SharedThis( this ), MenuBuilder.MakeWidget(), CursorPos, FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu ));

	return TSharedPtr<SWidget>();
}

void SAnimNotifyTrack::CreateCommands()
{
	check(!AnimSequenceEditorActions.IsValid());
	AnimSequenceEditorActions = MakeShareable(new FUICommandList);

	const FAnimSequenceEditorCommands& Commands = FAnimSequenceEditorCommands::Get();

/*
	FUICommandList& ActionList = *AnimSequenceEditorActions;

	ActionList.MapAction( 
		Commands.DeleteNotify, 
		FExecuteAction::CreateRaw( this, &SAnimNotifyTrack::OnDeleteNotifyClicked )
		);*/

}

void SAnimNotifyTrack::OnCopyNotifyClicked(int32 NotifyIndex)
{
	OnCopyNotifies.ExecuteIfBound();
}

bool SAnimNotifyTrack::CanPasteAnimNotify() const
{
	FString PropertyString;
	const TCHAR* Buffer;
	float OriginalTime;
	float OriginalLength;
	return ReadNotifyPasteHeader(PropertyString, Buffer, OriginalTime, OriginalLength);
}

bool SAnimNotifyTrack::ReadNotifyPasteHeader(FString& OutPropertyString, const TCHAR*& OutBuffer, float& OutOriginalTime, float& OutOriginalLength) const
{
	OutBuffer = NULL;
	OutOriginalTime = -1.f;

	FPlatformMisc::ClipboardPaste(OutPropertyString);

	if( !OutPropertyString.IsEmpty() )
	{
		//Remove header text
		const FString HeaderString( TEXT("COPY_ANIMNOTIFYEVENT") );
		
		//Check for string identifier in order to determine whether the text represents an FAnimNotifyEvent.
		if(OutPropertyString.StartsWith( HeaderString ) && OutPropertyString.Len() > HeaderString.Len() )
		{
			int32 HeaderSize = HeaderString.Len();
			OutBuffer = *OutPropertyString;
			OutBuffer += HeaderSize;

			FString ReadLine;
			// Read the original time from the first notify
			FParse::Line(&OutBuffer, ReadLine);
			FParse::Value( *ReadLine, TEXT( "OriginalTime=" ), OutOriginalTime );
			FParse::Value( *ReadLine, TEXT( "OriginalLength=" ), OutOriginalLength );
			return true;
		}
	}

	return false;
}

void SAnimNotifyTrack::OnPasteNotifyClicked(ENotifyPasteMode::Type PasteMode, ENotifyPasteMultipleMode::Type MultiplePasteType)
{
	float ClickTime = PasteMode == ENotifyPasteMode::MousePosition ? LastClickedTime : -1.0f;
	OnPasteNotifies.ExecuteIfBound(this, ClickTime, PasteMode, MultiplePasteType);
}

void SAnimNotifyTrack::OnPasteNotifyTimeSet(const FText& TimeText, ETextCommit::Type CommitInfo)
{
	if ( CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus )
	{
		float PasteTime = FCString::Atof( *TimeText.ToString() );
		PasteTime = FMath::Clamp<float>(PasteTime, 0.f, Sequence->SequenceLength-0.01f);
		OnPasteNotify(PasteTime);
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::OnPasteNotify(float TimeToPasteAt, ENotifyPasteMultipleMode::Type MultiplePasteType)
{
	FString PropertyString;
	const TCHAR* Buffer;
	float OriginalTime;				// Time in the original track that the node was placed
	float OriginalLength;			// Length of the original anim sequence
	float Shift = 0.0f;				// The amount to shift the saved display time of subsequent nodes
	float ScaleMultiplier = 1.0f;	// Scale value to place nodes relatively in different sequences

	if( ReadNotifyPasteHeader(PropertyString, Buffer, OriginalTime, OriginalLength) )
	{
		// If we're using relative mode, calculate the scale between the two sequences
		if (MultiplePasteType == ENotifyPasteMultipleMode::Relative)
		{
			ScaleMultiplier = Sequence->SequenceLength / OriginalLength;
		}

		//Store object to undo pasting anim notify
		const FScopedTransaction Transaction( LOCTEXT("PasteNotifyEvent", "Paste Anim Notify") );

		Sequence->Modify();

		FString NotifyLine;
		while (FParse::Line(&Buffer, NotifyLine))
		{
			// Add a new Anim Notify Event structure to the array
			int32 NewNotifyIndex = Sequence->Notifies.AddZeroed();

			//Get array property
			UArrayProperty* ArrayProperty = NULL;
			uint8* PropData = Sequence->FindNotifyPropertyData(NewNotifyIndex, ArrayProperty);

			if (PropData && ArrayProperty)
			{
				ArrayProperty->Inner->ImportText(*NotifyLine, PropData, PPF_Copy, NULL);

				FAnimNotifyEvent& NewNotify = Sequence->Notifies[NewNotifyIndex];

				if (TimeToPasteAt >= 0.f)
				{
					Shift = TimeToPasteAt - (NewNotify.GetTime() * ScaleMultiplier);
					NewNotify.SetTime(TimeToPasteAt);

					// Don't want to paste subsequent nodes at this position
					TimeToPasteAt = -1.0f;
				}
				else
				{
					NewNotify.SetTime((NewNotify.GetTime() * ScaleMultiplier) + Shift);
					// Clamp into the bounds of the sequence
					NewNotify.SetTime(FMath::Clamp(NewNotify.GetTime(), 0.0f, Sequence->SequenceLength));
				}
				NewNotify.TriggerTimeOffset = GetTriggerTimeOffsetForType(Sequence->CalculateOffsetForNotify(NewNotify.GetTime()));
				NewNotify.TrackIndex = TrackIndex;

				// Get anim notify ptr of the new notify event array element (This ptr is the same as the source anim notify ptr)
				UAnimNotify* AnimNotifyPtr = NewNotify.Notify;
				if (AnimNotifyPtr)
				{
					//Create a new instance of anim notify subclass.
					UAnimNotify* NewAnimNotifyPtr = Cast<UAnimNotify>(StaticDuplicateObject(AnimNotifyPtr, Sequence, TEXT("None")));
					if (NewAnimNotifyPtr)
					{
						//Reassign new anim notify ptr to the pasted anim notify event.
						NewNotify.Notify = NewAnimNotifyPtr;
					}
				}

				UAnimNotifyState * AnimNotifyStatePtr = NewNotify.NotifyStateClass;
				if (AnimNotifyStatePtr)
				{
					//Create a new instance of anim notify subclass.
					UAnimNotifyState * NewAnimNotifyStatePtr = Cast<UAnimNotifyState>(StaticDuplicateObject(AnimNotifyStatePtr, Sequence, TEXT("None")));
					if (NewAnimNotifyStatePtr)
					{
						//Reassign new anim notify ptr to the pasted anim notify event.
						NewNotify.NotifyStateClass = NewAnimNotifyStatePtr;
					}

					if ((NewNotify.GetTime() + NewNotify.GetDuration()) > Sequence->SequenceLength)
					{
						NewNotify.SetDuration(Sequence->SequenceLength - NewNotify.GetTime());
					}
				}
			}
			else
			{
				//Remove newly created array element if pasting is not successful
				Sequence->Notifies.RemoveAt(NewNotifyIndex);
			}
		}

		OnDeselectAllNotifies.ExecuteIfBound();
		Sequence->MarkPackageDirty();
		OnUpdatePanel.ExecuteIfBound();
	}
}

void SAnimNotifyTrack::OnManageNotifies()
{
	TSharedPtr< FPersona > PersonalPin = PersonaPtr.Pin();
	if( PersonalPin.IsValid() )
	{
		PersonalPin->GetTabManager()->InvokeTab( FPersonaTabs::SkeletonAnimNotifiesID );
	}
}

void SAnimNotifyTrack::OnOpenNotifySource(UBlueprint* InSourceBlueprint) const
{
	FAssetEditorManager::Get().OpenEditorForAsset(InSourceBlueprint);
}

void SAnimNotifyTrack::SetTriggerWeight(const FText& TriggerWeight, ETextCommit::Type CommitInfo, int32 NotifyIndex)
{
	if ( CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus )
	{
		if ( AnimNotifies.IsValidIndex(NotifyIndex) )
		{
			float NewWeight = FMath::Max(FCString::Atof( *TriggerWeight.ToString() ), ZERO_ANIMWEIGHT_THRESH);
			AnimNotifies[NotifyIndex]->TriggerWeightThreshold = NewWeight;
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

bool SAnimNotifyTrack::IsSingleNodeSelected()
{
	return SelectedNotifyIndices.Num() == 1;
}

bool SAnimNotifyTrack::IsSingleNodeInClipboard()
{
	FString PropString;
	const TCHAR* Buffer;
	float OriginalTime;
	float OriginalLength;
	uint32 Count = 0;
	if (ReadNotifyPasteHeader(PropString, Buffer, OriginalTime, OriginalLength))
	{
		// If reading a single line empties the buffer then we only have one notify in there.
		FString TempLine;
		FParse::Line(&Buffer, TempLine);
		return *Buffer == 0;
	}
	return false;
}

void SAnimNotifyTrack::OnSetTriggerWeightNotifyClicked(int32 NotifyIndex)
{
	if (AnimNotifies.IsValidIndex(NotifyIndex))
	{
		FString DefaultText = FString::Printf(TEXT("%0.6f"), AnimNotifies[NotifyIndex]->TriggerWeightThreshold);

		// Show dialog to enter weight
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label( LOCTEXT("TriggerWeightNotifyClickedLabel", "Trigger Weight") )
			.DefaultText( FText::FromString(DefaultText) )
			.OnTextCommitted( this, &SAnimNotifyTrack::SetTriggerWeight, NotifyIndex );

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
			);
	}
}

void SAnimNotifyTrack::OnSetDurationNotifyClicked(int32 NotifyIndex)
{
	if (AnimNotifies.IsValidIndex(NotifyIndex))
	{
		FString DefaultText = FString::Printf(TEXT("%f"), AnimNotifies[NotifyIndex]->GetDuration());

		// Show dialog to enter weight
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label(LOCTEXT("DurationNotifyClickedLabel", "Duration"))
			.DefaultText( FText::FromString(DefaultText) )
			.OnTextCommitted( this, &SAnimNotifyTrack::SetDuration, NotifyIndex );

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
			);
	}
}

void SAnimNotifyTrack::SetDuration(const FText& DurationText, ETextCommit::Type CommitInfo, int32 NotifyIndex)
{
	if ( CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus )
	{
		if ( AnimNotifies.IsValidIndex(NotifyIndex) )
		{
			float NewDuration = FMath::Max(FCString::Atof( *DurationText.ToString() ), SAnimNotifyNode::MinimumStateDuration);
			float MaxDuration = Sequence->SequenceLength - AnimNotifies[NotifyIndex]->GetTime();
			NewDuration = FMath::Min(NewDuration, MaxDuration);
			AnimNotifies[NotifyIndex]->SetDuration(NewDuration);

			// If we have a delegate bound to refresh the offsets, call it.
			// This is used by the montage editor to keep the offsets up to date.
			OnRequestRefreshOffsets.ExecuteIfBound();
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::OnNewNotifyClicked()
{
	// Show dialog to enter new track name
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label( LOCTEXT("NewNotifyLabel", "Notify Name") )
		.OnTextCommitted( this, &SAnimNotifyTrack::AddNewNotify );

	// Show dialog to enter new event name
	FSlateApplication::Get().PushMenu(
		AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
		);
}

void SAnimNotifyTrack::AddNewNotify(const FText& NewNotifyName, ETextCommit::Type CommitInfo)
{
	USkeleton* SeqSkeleton = Sequence->GetSkeleton();
	if ((CommitInfo == ETextCommit::OnEnter) && SeqSkeleton)
	{
		const FScopedTransaction Transaction( LOCTEXT("AddNewNotifyEvent", "Add New Anim Notify") );
		FName NewName = FName( *NewNotifyName.ToString() );
		SeqSkeleton->AddNewAnimationNotify(NewName);

		FBlueprintActionDatabase::Get().RefreshAssetActions(SeqSkeleton);

		CreateNewNotifyAtCursor(NewNotifyName.ToString(), (UClass*)nullptr);
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::Update()
{
	NotifyNodes.Empty();

	TrackArea->SetContent(
		SAssignNew( NodeSlots, SOverlay )
		);

	if ( AnimNotifies.Num() > 0 )
	{
		for (int32 NotifyIndex = 0; NotifyIndex < AnimNotifies.Num(); ++NotifyIndex)
		{
			FAnimNotifyEvent* Event = AnimNotifies[NotifyIndex];

			TSharedPtr<SAnimNotifyNode> AnimNotifyNode;
			
			NodeSlots->AddSlot()
			.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SAnimNotifyTrack::GetNotifyTrackPadding, NotifyIndex)))
			.VAlign(VAlign_Center)
			[
				SAssignNew( AnimNotifyNode, SAnimNotifyNode )
				.Sequence(Sequence)
				.AnimNotify(Event)
				.OnNodeDragStarted(this, &SAnimNotifyTrack::OnNotifyNodeDragStarted, NotifyIndex)
				.OnUpdatePanel(OnUpdatePanel)
				.PanTrackRequest(OnRequestTrackPan)
				.ViewInputMin(ViewInputMin)
				.ViewInputMax(ViewInputMax)
				.MarkerBars(MarkerBars)
				.OnDeselectAllNotifies(OnDeselectAllNotifies)
			];

			NotifyNodes.Add(AnimNotifyNode);
		}
	}
}

int32 SAnimNotifyTrack::GetHitNotifyNode(const FGeometry& MyGeometry, const FVector2D& CursorPosition)
{
	for ( int32 I=NotifyNodes.Num()-1; I>=0; --I ) //Run through from 'top most' Notify to bottom
	{
		if ( NotifyNodes[I].Get()->HitTest(MyGeometry, CursorPosition) )
		{
			return I;
		}
	}

	return INDEX_NONE;
}

FReply SAnimNotifyTrack::OnNotifyNodeDragStarted( TSharedRef<SAnimNotifyNode> NotifyNode, const FPointerEvent& MouseEvent, const FVector2D& ScreenNodePosition, const bool bDragOnMarker, int32 NotifyIndex ) 
{
	// Check to see if we've already selected the triggering node
	if (!NotifyNode->bSelected)
	{
		SelectNotifyNode(NotifyIndex, MouseEvent.IsShiftDown());
	}
	
	// Sort our nodes so we're acessing them in time order
	SelectedNotifyIndices.Sort([this](const int32& A, const int32& B)
	{
		FAnimNotifyEvent* First = AnimNotifies[A];
		FAnimNotifyEvent* Second = AnimNotifies[B];
		return First->GetTime() < Second->GetTime();
	});

	// If we're dragging one of the direction markers we don't need to call any further as we don't want the drag drop op
	if(!bDragOnMarker)
	{
		TArray<TSharedPtr<SAnimNotifyNode>> NodesToDrag;
		const float FirstNodeX = NotifyNodes[SelectedNotifyIndices[0]]->GetWidgetPosition().X;
		
		TSharedRef<SOverlay> DragBox = SNew(SOverlay);
		for (auto Iter = SelectedNotifyIndices.CreateIterator(); Iter; ++Iter)
		{
			TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[*Iter];
			NodesToDrag.Add(Node);
		}
		
		FVector2D DecoratorPosition = NodesToDrag[0]->GetWidgetPosition();
		DecoratorPosition = CachedGeometry.LocalToAbsolute(DecoratorPosition);
		return OnNodeDragStarted.Execute(NodesToDrag, DragBox, MouseEvent.GetScreenSpacePosition(), DecoratorPosition, bDragOnMarker);
	}
	else
	{
		// Capture the mouse in the node
		return FReply::Handled().CaptureMouse(NotifyNode).UseHighPrecisionMouseMovement(NotifyNode);
	}
}

FReply SAnimNotifyTrack::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
		CursorPos = MyGeometry.AbsoluteToLocal(CursorPos);
		int32 HitIndex = GetHitNotifyNode(MyGeometry, CursorPos);

		if (HitIndex!=INDEX_NONE)
		{
			// Hit a node, record the mouse position for use later so we can know when / where a
			// drag happened on the node handles if necessary.
			NotifyNodes[HitIndex]->SetLastMouseDownPosition(CursorPos);
			
			return FReply::Handled().DetectDrag( NotifyNodes[HitIndex].ToSharedRef(), EKeys::LeftMouseButton );
		}
	}

	return FReply::Unhandled();
}

float SAnimNotifyTrack::CalculateTime( const FGeometry& MyGeometry, FVector2D NodePos, bool bInputIsAbsolute )
{
	if(bInputIsAbsolute)
	{
		NodePos = MyGeometry.AbsoluteToLocal(NodePos);
	}
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, MyGeometry.Size);
	return FMath::Clamp<float>( ScaleInfo.LocalXToInput(NodePos.X), 0.f, Sequence->SequenceLength );
}

FReply SAnimNotifyTrack::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return FReply::Unhandled();
}

void SAnimNotifyTrack::HandleNodeDrop(TSharedPtr<SAnimNotifyNode> Node, float Offset)
{
	ensure(Node.IsValid());

	OnDeselectAllNotifies.ExecuteIfBound();
	const FScopedTransaction Transaction(LOCTEXT("MoveNotifyEvent", "Move Anim Notify"));
	Sequence->Modify();

	float LocalX = GetCachedGeometry().AbsoluteToLocal(Node->GetScreenPosition() + Offset).X;
	float Time = GetCachedScaleInfo().LocalXToInput(LocalX);
	FAnimNotifyEvent* DroppedEvent = Node->NotifyEvent;
	float EventDuration = DroppedEvent->GetDuration();

	if(Node->GetLastSnappedTime() != -1.0f)
	{
		DroppedEvent->Link(Sequence, Node->GetLastSnappedTime(), DroppedEvent->GetSlotIndex());
	}
	else
	{
		DroppedEvent->Link(Sequence, Time, DroppedEvent->GetSlotIndex());
	}
	DroppedEvent->RefreshTriggerOffset(Sequence->CalculateOffsetForNotify(DroppedEvent->GetTime()));

	if(EventDuration > 0.0f)
	{
		DroppedEvent->EndLink.Link(Sequence, DroppedEvent->GetTime() + EventDuration, DroppedEvent->GetSlotIndex());
		DroppedEvent->RefreshEndTriggerOffset(Sequence->CalculateOffsetForNotify(DroppedEvent->EndLink.GetTime()));
	}
	else
	{
		DroppedEvent->EndTriggerTimeOffset = 0.0f;
	}

	DroppedEvent->TrackIndex = TrackIndex;
}

void SAnimNotifyTrack::DisconnectSelectedNodesForDrag(TArray<TSharedPtr<SAnimNotifyNode>>& DragNodes)
{
	if(SelectedNotifyIndices.Num() == 0)
	{
		return;
	}

	const float FirstNodeX = NotifyNodes[SelectedNotifyIndices[0]]->GetWidgetPosition().X;

	for(auto Iter = SelectedNotifyIndices.CreateIterator(); Iter; ++Iter)
	{
		TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[*Iter];
		NodeSlots->RemoveSlot(Node->AsShared());

		DragNodes.Add(Node);
	}
}

void SAnimNotifyTrack::AppendSelectionToSet(FGraphPanelSelectionSet& SelectionSet)
{
	// Add our selection to the provided set
	for(auto Iter = SelectedNotifyIndices.CreateConstIterator(); Iter; ++Iter)
	{
		int32 Index = *Iter;
		check(AnimNotifies.IsValidIndex(Index));
		FAnimNotifyEvent* Event = AnimNotifies[Index];

		if(Event->Notify)
		{
			SelectionSet.Add(Event->Notify);
		}
		else if(Event->NotifyStateClass)
		{
			SelectionSet.Add(Event->NotifyStateClass);
		}
	}
}

void SAnimNotifyTrack::AppendSelectionToArray(TArray<FAnimNotifyEvent*>& Selection) const
{
	for(int32 Idx : SelectedNotifyIndices)
	{
		check(AnimNotifies.IsValidIndex(Idx));
		FAnimNotifyEvent* Event = AnimNotifies[Idx];

		if(Event)
		{
			Selection.Add(Event);
		}
	}
}

void SAnimNotifyTrack::PasteSingleNotify(FString& NotifyString, float PasteTime)
{
	int32 NewIdx = Sequence->Notifies.Add(FAnimNotifyEvent());
	UArrayProperty* ArrayProperty = NULL;
	uint8* PropertyData = Sequence->FindNotifyPropertyData(NewIdx, ArrayProperty);

	if(PropertyData && ArrayProperty)
	{
		ArrayProperty->Inner->ImportText(*NotifyString, PropertyData, PPF_Copy, NULL);

		FAnimNotifyEvent& NewNotify = Sequence->Notifies[NewIdx];

		if(PasteTime != -1.0f)
		{
			NewNotify.SetTime(PasteTime);
		}

		// Make sure the notify is within the track area
		NewNotify.SetTime(FMath::Clamp(NewNotify.GetTime(), 0.0f, Sequence->SequenceLength));
		NewNotify.TriggerTimeOffset = GetTriggerTimeOffsetForType(Sequence->CalculateOffsetForNotify(NewNotify.GetTime()));
		NewNotify.TrackIndex = TrackIndex;

		if(NewNotify.Notify)
		{
			UAnimNotify* NewNotifyObject = Cast<UAnimNotify>(StaticDuplicateObject(NewNotify.Notify, Sequence, TEXT("None")));
			check(NewNotifyObject);
			NewNotify.Notify = NewNotifyObject;
		}
		else if(NewNotify.NotifyStateClass)
		{
			UAnimNotifyState* NewNotifyStateObject = Cast<UAnimNotifyState>(StaticDuplicateObject(NewNotify.NotifyStateClass, Sequence, TEXT("None")));
			check(NewNotifyStateObject);
			NewNotify.NotifyStateClass = NewNotifyStateObject;

			// Clamp duration into the sequence
			NewNotify.SetDuration(FMath::Clamp(NewNotify.GetDuration(), 1 / 30.0f, Sequence->SequenceLength - NewNotify.GetTime()));
			NewNotify.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Sequence->CalculateOffsetForNotify(NewNotify.GetTime() + NewNotify.GetDuration()));
		}

		NewNotify.ConditionalRelink();
	}
	else
	{
		// Paste failed, remove the notify
		Sequence->Notifies.RemoveAt(NewIdx);
	}

	OnDeselectAllNotifies.ExecuteIfBound();
	Sequence->MarkPackageDirty();
	OnUpdatePanel.ExecuteIfBound();
}

void SAnimNotifyTrack::AppendSelectedNodeWidgetsToArray(TArray<TSharedPtr<SAnimNotifyNode>>& NodeArray) const
{
	for(TSharedPtr<SAnimNotifyNode> Node : NotifyNodes)
	{
		if(Node->bSelected)
		{
			NodeArray.Add(Node);
		}
	}
}

void SAnimNotifyTrack::RefreshMarqueeSelectedNodes(FSlateRect& Rect, FNotifyMarqueeOperation& Marquee)
{
	if(Marquee.Operation != FNotifyMarqueeOperation::Replace)
	{
		// Maintain the original selection from before the operation
		for(int32 Idx = 0 ; Idx < NotifyNodes.Num() ; ++Idx)
		{
			TSharedPtr<SAnimNotifyNode> Notify = NotifyNodes[Idx];
			bool bWasSelected = Marquee.OriginalSelection.Contains(Notify);
			if(bWasSelected)
			{
				SelectNotifyNode(Idx, true, false);
			}
			else if(SelectedNotifyIndices.Contains(Idx))
			{
				DeselectNotifyNode(Idx, false);
			}
		}
	}

	for(int32 Index = 0 ; Index < NotifyNodes.Num() ; ++Index)
	{
		TSharedPtr<SAnimNotifyNode> Node = NotifyNodes[Index];
		FSlateRect NodeRect = FSlateRect(Node->GetWidgetPosition(), Node->GetWidgetPosition() + Node->GetSize());

		if(FSlateRect::DoRectanglesIntersect(Rect, NodeRect))
		{
			// Either select or deselect the intersecting node, depending on the type of selection operation
			if(Marquee.Operation == FNotifyMarqueeOperation::Remove)
			{
				if(SelectedNotifyIndices.Contains(Index))
				{
					DeselectNotifyNode(Index, false);
				}
			}
			else
			{
				SelectNotifyNode(Index, true, false);
			}
		}
	}
}

FString SAnimNotifyTrack::MakeBlueprintNotifyName(FAssetData& NotifyAssetData)
{
	FString DefaultNotifyName = NotifyAssetData.AssetName.ToString();
	DefaultNotifyName = DefaultNotifyName.Replace(TEXT("AnimNotify_"), TEXT(""), ESearchCase::CaseSensitive);
	DefaultNotifyName = DefaultNotifyName.Replace(TEXT("AnimNotifyState_"), TEXT(""), ESearchCase::CaseSensitive);

	return DefaultNotifyName;
}

void SAnimNotifyTrack::GetNotifyMenuData(TArray<FAssetData>& NotifyAssetData, TArray<BlueprintNotifyMenuInfo>& OutNotifyMenuData)
{
	for(FAssetData& NotifyData : NotifyAssetData)
	{
		OutNotifyMenuData.AddZeroed();
		BlueprintNotifyMenuInfo& MenuInfo = OutNotifyMenuData.Last();

		MenuInfo.BlueprintPath = NotifyData.ObjectPath.ToString();
		MenuInfo.NotifyName = MakeBlueprintNotifyName(NotifyData);
	}

	OutNotifyMenuData.Sort([](const BlueprintNotifyMenuInfo& A, const BlueprintNotifyMenuInfo& B)
	{
		return A.NotifyName < B.NotifyName;
	});
}

void SAnimNotifyTrack::OnSetNotifyTimeClicked(int32 NotifyIndex)
{
	if(AnimNotifies.IsValidIndex(NotifyIndex))
	{
		FString DefaultText = FString::Printf(TEXT("%0.6f"), AnimNotifies[NotifyIndex]->GetTime());

		// Show dialog to enter time
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label(LOCTEXT("NotifyTimeClickedLabel", "Notify Time"))
			.DefaultText(FText::FromString(DefaultText))
			.OnTextCommitted(this, &SAnimNotifyTrack::SetNotifyTime, NotifyIndex);

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}
}

void SAnimNotifyTrack::SetNotifyTime(const FText& NotifyTimeText, ETextCommit::Type CommitInfo, int32 NotifyIndex)
{
	if(CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		if(AnimNotifies.IsValidIndex(NotifyIndex))
		{
			FAnimNotifyEvent* Event = AnimNotifies[NotifyIndex];

			float NewTime = FMath::Clamp(FCString::Atof(*NotifyTimeText.ToString()), 0.0f, Sequence->SequenceLength - Event->GetDuration());

			Event->SetTime(NewTime);

			Event->RefreshTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime()));
			if(Event->GetDuration() > 0.0f)
			{
				Event->RefreshEndTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime() + Event->GetDuration()));
			}
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SAnimNotifyTrack::OnSetNotifyFrameClicked(int32 NotifyIndex)
{
	if(AnimNotifies.IsValidIndex(NotifyIndex))
	{
		FAnimNotifyEvent* Event = AnimNotifies[NotifyIndex];

		//const float Time = Event->GetTime();
		//const float Percentage = Time / Sequence->SequenceLength;
		const FText Frame = FText::AsNumber(Sequence->GetFrameAtTime(Event->GetTime()));

		FString DefaultText = FString::Printf(TEXT("%s"), *Frame.ToString());

		// Show dialog to enter frame
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label(LOCTEXT("NotifyFrameClickedLabel", "Notify Frame"))
			.DefaultText(FText::FromString(DefaultText))
			.OnTextCommitted(this, &SAnimNotifyTrack::SetNotifyFrame, NotifyIndex);

		FSlateApplication::Get().PushMenu(
			AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}
}

void SAnimNotifyTrack::SetNotifyFrame(const FText& NotifyFrameText, ETextCommit::Type CommitInfo, int32 NotifyIndex)
{
	if(CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		if(AnimNotifies.IsValidIndex(NotifyIndex))
		{
			FAnimNotifyEvent* Event = AnimNotifies[NotifyIndex];

			int32 Frame = FCString::Atof(*NotifyFrameText.ToString());
			float NewTime = FMath::Clamp(Sequence->GetTimeAtFrame(Frame), 0.0f, Sequence->SequenceLength - Event->Duration);

			Event->SetTime(NewTime);

			Event->RefreshTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime()));
			if(Event->GetDuration() > 0.0f)
			{
				Event->RefreshEndTriggerOffset(Sequence->CalculateOffsetForNotify(Event->GetTime() + Event->GetDuration()));
			}
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

//////////////////////////////////////////////////////////////////////////
// SSequenceEdTrack

void SNotifyEdTrack::Construct(const FArguments& InArgs)
{
	Sequence = InArgs._Sequence;
	TrackIndex = InArgs._TrackIndex;
	FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[InArgs._TrackIndex];
	// @Todo anim: we need to fix this to allow track color to be customizable. 
	// for now name, and track color are given
	Track.TrackColor = ((TrackIndex & 1) != 0) ? FLinearColor(0.9f, 0.9f, 0.9f, 0.9f) : FLinearColor(0.5f, 0.5f, 0.5f);

	TSharedRef<SAnimNotifyPanel> PanelRef = InArgs._AnimNotifyPanel.ToSharedRef();
	AnimPanelPtr = InArgs._AnimNotifyPanel;

	//////////////////////////////
	this->ChildSlot
	[
		SNew( SBorder )
		.Padding( FMargin(2.0f, 2.0f) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				// Notification editor panel
				SAssignNew(NotifyTrack, SAnimNotifyTrack)
				.Persona(PanelRef->GetPersona().Pin())
				.Sequence(Sequence)
				.TrackIndex(TrackIndex)
				.AnimNotifies(Track.Notifies)
				.ViewInputMin(InArgs._ViewInputMin)
				.ViewInputMax(InArgs._ViewInputMax)
				.OnSelectionChanged(InArgs._OnSelectionChanged)
				.OnUpdatePanel(InArgs._OnUpdatePanel)
				.OnGetNotifyBlueprintData(InArgs._OnGetNotifyBlueprintData)
				.OnGetNotifyStateBlueprintData(InArgs._OnGetNotifyStateBlueprintData)
				.OnGetNotifyNativeClasses(InArgs._OnGetNotifyNativeClasses)
				.OnGetNotifyStateNativeClasses(InArgs._OnGetNotifyStateNativeClasses)
				.OnGetScrubValue(InArgs._OnGetScrubValue)
				.OnGetDraggedNodePos(InArgs._OnGetDraggedNodePos)
				.OnNodeDragStarted(InArgs._OnNodeDragStarted)
				.MarkerBars(InArgs._MarkerBars)
				.TrackColor(Track.TrackColor)
				.OnRequestTrackPan(FPanTrackRequest::CreateSP(PanelRef, &SAnimNotifyPanel::PanInputViewRange))
				.OnRequestOffsetRefresh(InArgs._OnRequestRefreshOffsets)
				.OnDeleteNotify(InArgs._OnDeleteNotify)
				.OnGetIsAnimNotifySelectionValidForReplacement(PanelRef, &SAnimNotifyPanel::IsNotifySelectionValidForReplacement)
				.OnReplaceSelectedWithNotify(PanelRef, &SAnimNotifyPanel::OnReplaceSelectedWithNotify)
				.OnReplaceSelectedWithBlueprintNotify(PanelRef, &SAnimNotifyPanel::OnReplaceSelectedWithNotifyBlueprint)
				.OnDeselectAllNotifies(InArgs._OnDeselectAllNotifies)
				.OnCopyNotifies(InArgs._OnCopyNotifies)
				.OnPasteNotifies(InArgs._OnPasteNotifies)
				.OnSetInputViewRange(InArgs._OnSetInputViewRange)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride(InArgs._WidgetWidth)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.FillWidth(1)
					[
						// Name of track
						SNew(STextBlock)
						.Text( FText::FromName( Track.TrackName ) )
						.ColorAndOpacity(Track.TrackColor)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Name of track
						SNew(SButton)
						.Text(LOCTEXT("AddTrackButtonLabel", "+"))
						.ToolTipText( LOCTEXT("AddTrackTooltip", "Add track above here") )
						.OnClicked( PanelRef, &SAnimNotifyPanel::InsertTrack, TrackIndex + 1 )
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Name of track
						SNew(SButton)
						.Text(LOCTEXT("RemoveTrackButtonLabel", "-"))
						.ToolTipText( LOCTEXT("RemoveTrackTooltip", "Remove this track") )
						.OnClicked( PanelRef, &SAnimNotifyPanel::DeleteTrack, TrackIndex )
						.IsEnabled( SNotifyEdTrack::CanDeleteTrack() )
					]
				]
			]
		]
	];
}

bool SNotifyEdTrack::CanDeleteTrack()
{
	return AnimPanelPtr.Pin()->CanDeleteTrack(TrackIndex);
}

//////////////////////////////////////////////////////////////////////////
// FAnimNotifyPanelCommands

void FAnimNotifyPanelCommands::RegisterCommands()
{
	UI_COMMAND(DeleteNotify, "Delete", "Deletes the selected notifies.", EUserInterfaceActionType::Button, FInputChord(EKeys::Platform_Delete));
}

//////////////////////////////////////////////////////////////////////////
// SAnimNotifyPanel

void SAnimNotifyPanel::Construct(const FArguments& InArgs)
{
	SAnimTrackPanel::Construct( SAnimTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	PersonaPtr = InArgs._Persona;
	Sequence = InArgs._Sequence;
	MarkerBars = InArgs._MarkerBars;

	FAnimNotifyPanelCommands::Register();
	BindCommands();

	// @todo anim : this is kinda hack to make sure it has 1 track is alive
	// we can do this whenever import or asset is created, but it's more places to handle than here
	// the function name in that case will need to change
	Sequence->InitializeNotifyTrack();
	Sequence->RegisterOnNotifyChanged(UAnimSequenceBase::FOnNotifyChanged::CreateSP( this, &SAnimNotifyPanel::RefreshNotifyTracks )  );
	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SAnimNotifyPanel::PostUndo ) );
	PersonaPtr.Pin()->RegisterOnGenericDelete(FPersona::FOnDeleteGeneric::CreateSP(this, &SAnimNotifyPanel::OnDeletePressed));

	CurrentPosition = InArgs._CurrentPosition;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	WidgetWidth = InArgs._WidgetWidth;
	OnGetScrubValue = InArgs._OnGetScrubValue;
	OnRequestRefreshOffsets = InArgs._OnRequestRefreshOffsets;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SExpandableArea )
			.AreaTitle( LOCTEXT("Notifies", "Notifies") )
			.AddMetaData<FTagMetaData>(TEXT("AnimNotify.Notify"))
			.BodyContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					SAssignNew(PanelArea, SBorder)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.Padding(FMargin(2.0f, 2.0f))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SAssignNew(NotifyTrackScrollBar, SScrollBar)
						.Orientation(EOrientation::Orient_Horizontal)
						.AlwaysShowScrollbar(true)
						.OnUserScrolled(this, &SAnimNotifyPanel::OnNotifyTrackScrolled)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(WidgetWidth)
					]
				]
			]
		]
	];

	InputViewRangeChanged(ViewInputMin.Get(), ViewInputMax.Get());

	OnPropertyChangedHandle = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateSP(this, &SAnimNotifyPanel::OnPropertyChanged);
	OnPropertyChangedHandleDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedHandle);

	// Base notify classes used to search asset data for children.
	NotifyClassNames.Add(TEXT("Class'/Script/Engine.AnimNotify'"));
	NotifyStateClassNames.Add(TEXT("Class'/Script/Engine.AnimNotifyState'"));

	PopulateNotifyBlueprintClasses(NotifyClassNames);
	PopulateNotifyBlueprintClasses(NotifyStateClassNames);

	Update();
}

SAnimNotifyPanel::~SAnimNotifyPanel()
{
	Sequence->UnregisterOnNotifyChanged(this);
	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo(this);
		PersonaPtr.Pin()->UnregisterOnGenericDelete(this);
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandleDelegateHandle);
}

FReply SAnimNotifyPanel::InsertTrack(int32 TrackIndexToInsert)
{
	// before insert, make sure everything behind is fixed
	for (int32 I=TrackIndexToInsert; I<Sequence->AnimNotifyTracks.Num(); ++I)
	{
		FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[I];
		for (int32 J=0; J<Track.Notifies.Num(); ++J)
		{
			// fix notifies indices
			Track.Notifies[J]->TrackIndex = I+1;
		}

		// fix track names, for now it's only index
		Track.TrackName = *FString::FromInt(I+2);
	}

	FAnimNotifyTrack NewItem;
	NewItem.TrackName = *FString::FromInt(TrackIndexToInsert+1);
	NewItem.TrackColor = FLinearColor::White;

	Sequence->AnimNotifyTracks.Insert(NewItem, TrackIndexToInsert);
	Sequence->MarkPackageDirty();

	Update();
	return FReply::Handled();
}

FReply SAnimNotifyPanel::DeleteTrack(int32 TrackIndexToDelete)
{
	if (Sequence->AnimNotifyTracks.IsValidIndex(TrackIndexToDelete))
	{
		if (Sequence->AnimNotifyTracks[TrackIndexToDelete].Notifies.Num() == 0)
		{
			// before insert, make sure everything behind is fixed
			for (int32 I=TrackIndexToDelete+1; I<Sequence->AnimNotifyTracks.Num(); ++I)
			{
				FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[I];
				for (int32 J=0; J<Track.Notifies.Num(); ++J)
				{
					// fix notifies indices
					Track.Notifies[J]->TrackIndex = I-1;
				}

				// fix track names, for now it's only index
				Track.TrackName = *FString::FromInt(I);
			}

			Sequence->AnimNotifyTracks.RemoveAt(TrackIndexToDelete);
			Sequence->MarkPackageDirty();
			Update();
		}
	}
	return FReply::Handled();
}

bool SAnimNotifyPanel::CanDeleteTrack(int32 TrackIndexToDelete)
{
	if (Sequence->AnimNotifyTracks.Num() > 1 && Sequence->AnimNotifyTracks.IsValidIndex(TrackIndexToDelete))
	{
		return Sequence->AnimNotifyTracks[TrackIndexToDelete].Notifies.Num() == 0;
	}

	return false;
}

void SAnimNotifyPanel::DeleteNotify(FAnimNotifyEvent* Notify)
{
	for (int32 I=0; I<Sequence->Notifies.Num(); ++I)
	{
		if (Notify == &(Sequence->Notifies[I]))
		{
			Sequence->Notifies.RemoveAt(I);
			Sequence->MarkPackageDirty();
			break;
		}
	}
}

void SAnimNotifyPanel::Update()
{
	PersonaPtr.Pin()->OnAnimNotifiesChanged.Broadcast();
	if(Sequence != NULL)
	{
		Sequence->UpdateAnimNotifyTrackCache();
	}
}

bool SAnimNotifyPanel::ReadNotifyPasteHeader(FString& OutPropertyString, const TCHAR*& OutBuffer, float& OutOriginalTime, float& OutOriginalLength, int32& OutTrackSpan) const
{
	OutBuffer = NULL;
	OutOriginalTime = -1.f;

	FPlatformMisc::ClipboardPaste(OutPropertyString);

	if(!OutPropertyString.IsEmpty())
	{
		//Remove header text
		const FString HeaderString(TEXT("COPY_ANIMNOTIFYEVENT"));

		//Check for string identifier in order to determine whether the text represents an FAnimNotifyEvent.
		if(OutPropertyString.StartsWith(HeaderString) && OutPropertyString.Len() > HeaderString.Len())
		{
			int32 HeaderSize = HeaderString.Len();
			OutBuffer = *OutPropertyString;
			OutBuffer += HeaderSize;

			FString ReadLine;
			// Read the original time from the first notify
			FParse::Line(&OutBuffer, ReadLine);
			FParse::Value(*ReadLine, TEXT("OriginalTime="), OutOriginalTime);
			FParse::Value(*ReadLine, TEXT("OriginalLength="), OutOriginalLength);
			FParse::Value(*ReadLine, TEXT("TrackSpan="), OutTrackSpan);
			return true;
		}
	}

	return false;
}

void SAnimNotifyPanel::RefreshNotifyTracks()
{
	check (Sequence);

	TSharedPtr<SVerticalBox> NotifySlots;
	PanelArea->SetContent(
		SAssignNew( NotifySlots, SVerticalBox )
		);

	NotifyAnimTracks.Empty();

	for(int32 I=Sequence->AnimNotifyTracks.Num()-1; I>=0; --I)
	{
		FAnimNotifyTrack& Track = Sequence->AnimNotifyTracks[I];
		TSharedPtr<SNotifyEdTrack> EdTrack;

		NotifySlots->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			SAssignNew(EdTrack, SNotifyEdTrack)
			.TrackIndex(I)
			.Sequence(Sequence)
			.AnimNotifyPanel(SharedThis(this))
			.WidgetWidth(WidgetWidth)
			.ViewInputMin(ViewInputMin)
			.ViewInputMax(ViewInputMax)
			.OnGetScrubValue(OnGetScrubValue)
			.OnGetDraggedNodePos(this, &SAnimNotifyPanel::CalculateDraggedNodePos)
			.OnUpdatePanel(this, &SAnimNotifyPanel::Update)
			.OnGetNotifyBlueprintData(this, &SAnimNotifyPanel::OnGetNotifyBlueprintData, &NotifyClassNames)
			.OnGetNotifyStateBlueprintData(this, &SAnimNotifyPanel::OnGetNotifyBlueprintData, &NotifyStateClassNames)
			.OnGetNotifyNativeClasses(this, &SAnimNotifyPanel::OnGetNativeNotifyData, UAnimNotify::StaticClass(), &NotifyClassNames)
			.OnGetNotifyStateNativeClasses(this, &SAnimNotifyPanel::OnGetNativeNotifyData, UAnimNotifyState::StaticClass(), &NotifyStateClassNames)
			.OnSelectionChanged(this, &SAnimNotifyPanel::OnTrackSelectionChanged)
			.OnNodeDragStarted(this, &SAnimNotifyPanel::OnNotifyNodeDragStarted)
			.MarkerBars(MarkerBars)
			.OnRequestRefreshOffsets(OnRequestRefreshOffsets)
			.OnDeleteNotify(this, &SAnimNotifyPanel::DeleteSelectedNotifies)
			.OnDeselectAllNotifies(this, &SAnimNotifyPanel::DeselectAllNotifies)
			.OnCopyNotifies(this, &SAnimNotifyPanel::CopySelectedNotifiesToClipboard)
			.OnPasteNotifies(this, &SAnimNotifyPanel::OnPasteNotifies)
			.OnSetInputViewRange(this, &SAnimNotifyPanel::InputViewRangeChanged)
		];

		NotifyAnimTracks.Add(EdTrack->NotifyTrack);
	}
}

float SAnimNotifyPanel::CalculateDraggedNodePos() const
{
	return CurrentDragXPosition;
}

FReply SAnimNotifyPanel::OnNotifyNodeDragStarted(TArray<TSharedPtr<SAnimNotifyNode>> NotifyNodes, TSharedRef<SWidget> Decorator, const FVector2D& ScreenCursorPos, const FVector2D& ScreenNodePosition, const bool bDragOnMarker)
{
	TSharedRef<SOverlay> NodeDragDecorator = SNew(SOverlay);
	TArray<TSharedPtr<SAnimNotifyNode>> Nodes;

	for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->DisconnectSelectedNodesForDrag(Nodes);
	}

	FVector2D OverlayOrigin = Nodes[0]->GetScreenPosition();
	FVector2D OverlayExtents = OverlayOrigin;
	OverlayExtents.X += Nodes[0]->GetDurationSize();
	for(int32 Idx = 1 ; Idx < Nodes.Num() ; ++Idx)
	{
		TSharedPtr<SAnimNotifyNode> Node = Nodes[Idx];
		FVector2D NodePosition = Node->GetScreenPosition();
		float NodeDuration = Node->GetDurationSize();

		if(NodePosition.X < OverlayOrigin.X)
		{
			OverlayOrigin.X = NodePosition.X;
		}
		else if(NodePosition.X + NodeDuration > OverlayExtents.X)
		{
			OverlayExtents.X = NodePosition.X + NodeDuration;
		}

		if(NodePosition.Y < OverlayOrigin.Y)
		{
			OverlayOrigin.Y = NodePosition.Y;
		}
		else if(NodePosition.Y + NotifyHeight > OverlayExtents.Y)
		{
			OverlayExtents.Y = NodePosition.Y + NotifyHeight;
		}
	}
	OverlayExtents -= OverlayOrigin;

	for(TSharedPtr<SAnimNotifyNode> Node : Nodes)
	{
		FVector2D OffsetFromFirst(Node->GetScreenPosition() - OverlayOrigin);

		NodeDragDecorator->AddSlot()
			.Padding(FMargin(OffsetFromFirst.X, OffsetFromFirst.Y, 0.0f, 0.0f))
			[
				Node->AsShared()
			];
	}

	FPanTrackRequest PanRequestDelegate = FPanTrackRequest::CreateSP(this, &SAnimNotifyPanel::PanInputViewRange);
	FOnUpdatePanel UpdateDelegate = FOnUpdatePanel::CreateSP(this, &SAnimNotifyPanel::Update);
	return FReply::Handled().BeginDragDrop(FNotifyDragDropOp::New(Nodes, NodeDragDecorator, NotifyAnimTracks, Sequence, ScreenCursorPos, OverlayOrigin, OverlayExtents, CurrentDragXPosition, PanRequestDelegate, MarkerBars, UpdateDelegate));
}

void SAnimNotifyPanel::PostUndo()
{
	if(Sequence != NULL)
	{
		Sequence->UpdateAnimNotifyTrackCache();
	}
}

void SAnimNotifyPanel::OnDeletePressed()
{
	// If there's no focus on the panel it's likely the user is not editing notifies
	// so don't delete anything when the key is pressed.
	if(HasKeyboardFocus() || HasFocusedDescendants()) 
	{
		DeleteSelectedNotifies();
	}
}

void SAnimNotifyPanel::DeleteSelectedNotifies()
{
	TArray<FAnimNotifyEvent*> SelectedNotifies;
	for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNotifies);
	}

	if(SelectedNotifies.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("DeleteNotifies", "Delete Animation Notifies"));
		Sequence->Modify(true);

		// Delete from highest index to lowest
		SelectedNotifies.Sort();
		for(int NotifyIndex = SelectedNotifies.Num() - 1; NotifyIndex >= 0 ; --NotifyIndex)
		{
			FAnimNotifyEvent* Notify = SelectedNotifies[NotifyIndex];
			DeleteNotify(Notify);
		}
	}

	// clear selection and update the panel
	FGraphPanelSelectionSet ObjectSet;
	OnSelectionChanged.ExecuteIfBound(ObjectSet);

	Update();
}

void SAnimNotifyPanel::SetSequence(class UAnimSequenceBase *	InSequence)
{
	if (InSequence != Sequence)
	{
		Sequence = InSequence;
		// @todo anim : this is kinda hack to make sure it has 1 track is alive
		// we can do this whenever import or asset is created, but it's more places to handle than here
		// the function name in that case will need to change
		Sequence->InitializeNotifyTrack();
		Update();
	}
}

void SAnimNotifyPanel::OnTrackSelectionChanged()
{
	// Need to collect selection info from all tracks
	FGraphPanelSelectionSet SelectionSet;
	TArray<UEditorNotifyObject*> NotifyObjects;

	TArray<FAnimNotifyEvent*> Events;
	for(int32 TrackIdx = 0 ; TrackIdx < NotifyAnimTracks.Num() ; ++TrackIdx)
	{
		TSharedPtr<SAnimNotifyTrack> Track = NotifyAnimTracks[TrackIdx];
		TArray<int32> TrackIndices = Track->GetSelectedNotifyIndices();
		for(int32 Idx : TrackIndices)
		{
			FString ObjName = MakeUniqueObjectName(GetTransientPackage(), UEditorNotifyObject::StaticClass()).ToString();
			UEditorNotifyObject* NewNotifyObject = NewObject<UEditorNotifyObject>(GetTransientPackage(), FName(*ObjName), RF_Public | RF_Standalone | RF_Transient);
			NewNotifyObject->InitFromAnim(Sequence, FOnAnimObjectChange::CreateSP(this, &SAnimNotifyPanel::OnNotifyObjectChanged));
			NewNotifyObject->InitialiseNotify(NotifyAnimTracks.Num() - TrackIdx - 1, Idx);
			SelectionSet.Add(NewNotifyObject);
		}
	}

	OnSelectionChanged.ExecuteIfBound(SelectionSet);
}

void SAnimNotifyPanel::DeselectAllNotifies()
{
	for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->DeselectAllNotifyNodes(false);
	}

	OnTrackSelectionChanged();
}

void SAnimNotifyPanel::CopySelectedNotifiesToClipboard() const
{
	// Grab the selected events
	TArray<FAnimNotifyEvent*> SelectedNotifies;
	for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNotifies);
	}

	const FString HeaderString(TEXT("COPY_ANIMNOTIFYEVENT"));
	
	if(SelectedNotifies.Num() > 0)
	{
		FString StrValue(HeaderString);

		// Sort by track
		SelectedNotifies.Sort([](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B)
		{
			return (A.TrackIndex > B.TrackIndex) || (A.TrackIndex == B.TrackIndex && A.GetTime() < B.GetTime());
		});

		// Need to find how many tracks this selection spans and the minimum time to use as the beginning of the selection
		int32 MinTrack = MAX_int32;
		int32 MaxTrack = MIN_int32;
		float MinTime = MAX_flt;
		for(const FAnimNotifyEvent* Event : SelectedNotifies)
		{
			MinTrack = FMath::Min(MinTrack, Event->TrackIndex);
			MaxTrack = FMath::Max(MaxTrack, Event->TrackIndex);
			MinTime = FMath::Min(MinTime, Event->GetTime());
		}

		int32 TrackSpan = MaxTrack - MinTrack + 1;

		StrValue += FString::Printf(TEXT("OriginalTime=%f,"), MinTime);
		StrValue += FString::Printf(TEXT("OriginalLength=%f,"), Sequence->SequenceLength);
		StrValue += FString::Printf(TEXT("TrackSpan=%d"), TrackSpan);

		for(const FAnimNotifyEvent* Event : SelectedNotifies)
		{
			// Locate the notify in the sequence, we need the sequence index; but also need to
			// keep the order we're currently in.
			int32 Index = INDEX_NONE;
			for(int32 NotifyIdx = 0 ; NotifyIdx < Sequence->Notifies.Num() ; ++NotifyIdx)
			{
				if(Event == &Sequence->Notifies[NotifyIdx])
				{
					Index = NotifyIdx;
					break;
				}
			}

			check(Index != INDEX_NONE);
			
			StrValue += "\n";
			UArrayProperty* ArrayProperty = NULL;
			uint8* PropertyData = Sequence->FindNotifyPropertyData(Index, ArrayProperty);
			StrValue += FString::Printf(TEXT("AbsTime=%f,"), Event->GetTime());
			if(PropertyData && ArrayProperty)
			{
				ArrayProperty->Inner->ExportTextItem(StrValue, PropertyData, PropertyData, Sequence, PPF_Copy);
			}

		}
		FPlatformMisc::ClipboardCopy(*StrValue);
	}
}

bool SAnimNotifyPanel::IsNotifySelectionValidForReplacement()
{
	// Grab the selected events
	TArray<FAnimNotifyEvent*> SelectedNotifies;
	for (TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNotifies);
	}

	bool bSelectionContainsAnimNotify = false;
	bool bSelectionContainsAnimNotifyState = false;
	for (FAnimNotifyEvent* NotifyEvent : SelectedNotifies)
	{
		if (NotifyEvent)
		{
			if (NotifyEvent->Notify)
			{
				bSelectionContainsAnimNotify = true;
			}
			else if (NotifyEvent->NotifyStateClass)
			{
				bSelectionContainsAnimNotifyState = true;
			}
			// Custom AnimNotifies have no class, but they are like AnimNotify class notifies in that they have no duration
			else
			{
				bSelectionContainsAnimNotify = true;
			}
		}
	}

	// Only allow replacement for selections that contain _only_ AnimNotifies, or _only_ AnimNotifyStates, but not both
	// (Want to disallow replacement of AnimNotify with AnimNotifyState, and vice-versa)
	bool bIsValidSelection = bSelectionContainsAnimNotify != bSelectionContainsAnimNotifyState;

	return bIsValidSelection;
}


void SAnimNotifyPanel::OnReplaceSelectedWithNotify(FString NewNotifyName, UClass* NewNotifyClass)
{
	TArray<FAnimNotifyEvent*> SelectedNotifies;
	for (TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
	{
		Track->AppendSelectionToArray(SelectedNotifies);
	}

	// Sort these since order is important for deletion
	SelectedNotifies.Sort();

	const FScopedTransaction Transaction(LOCTEXT("AddNotifyEvent", "Replace Anim Notify"));
	Sequence->Modify(true);

	for (int NotifyIndex = SelectedNotifies.Num()-1; NotifyIndex >= 0; --NotifyIndex)
	{
		FAnimNotifyEvent* OldEvent = SelectedNotifies[NotifyIndex];
		if (OldEvent)
		{
			float BeginTime = OldEvent->GetTime();
			float Length = OldEvent->GetDuration();
			int32 TargetTrackIndex = (NotifyAnimTracks.Num()-1) - OldEvent->TrackIndex;
			float TriggerTimeOffset = OldEvent->TriggerTimeOffset;
			float EndTriggerTimeOffset = OldEvent->EndTriggerTimeOffset;
			int32 SlotIndex = OldEvent->GetSlotIndex();
			int32 EndSlotIndex = OldEvent->EndLink.GetSlotIndex();
			int32 SegmentIndex = OldEvent->GetSegmentIndex();
			int32 EndSegmentIndex = OldEvent->GetSegmentIndex();
			EAnimLinkMethod::Type LinkMethod = OldEvent->GetLinkMethod();
			EAnimLinkMethod::Type EndLinkMethod = OldEvent->EndLink.GetLinkMethod();

			// Delete old one before creating new one to avoid potential array re-allocation when array temporarily increases by 1 in size
			DeleteNotify(OldEvent);
			FAnimNotifyEvent& NewEvent = NotifyAnimTracks[TargetTrackIndex]->CreateNewNotify(NewNotifyName, NewNotifyClass, BeginTime);

			NewEvent.TriggerTimeOffset = TriggerTimeOffset;
			NewEvent.ChangeSlotIndex(SlotIndex);
			NewEvent.SetSegmentIndex(SegmentIndex);
			NewEvent.ChangeLinkMethod(LinkMethod);

			// For Anim Notify States, handle the end time and link
			if (NewEvent.NotifyStateClass != nullptr)
			{
				NewEvent.SetDuration(Length);
				NewEvent.EndTriggerTimeOffset = EndTriggerTimeOffset;
				NewEvent.EndLink.ChangeSlotIndex(EndSlotIndex);
				NewEvent.EndLink.SetSegmentIndex(EndSegmentIndex);
				NewEvent.EndLink.ChangeLinkMethod(EndLinkMethod);
			}
						
			NewEvent.Update();
		}
	}

	// clear selection  
	FGraphPanelSelectionSet ObjectSet;
	OnSelectionChanged.ExecuteIfBound(ObjectSet);
	// TODO: set selection to new notifies?
	// update the panel
	Update();
}

void SAnimNotifyPanel::OnReplaceSelectedWithNotifyBlueprint(FString NewBlueprintNotifyName, FString NewBlueprintNotifyClass)
{
	TSubclassOf<UObject> BlueprintClass = SAnimNotifyTrack::GetBlueprintClassFromPath(NewBlueprintNotifyClass);
	OnReplaceSelectedWithNotify(NewBlueprintNotifyName, BlueprintClass);
}

void SAnimNotifyPanel::OnPasteNotifies(SAnimNotifyTrack* RequestTrack, float ClickTime, ENotifyPasteMode::Type PasteMode, ENotifyPasteMultipleMode::Type MultiplePasteType)
{
	int32 PasteIdx = RequestTrack->GetTrackIndex();
	int32 NumTracks = NotifyAnimTracks.Num();
	FString PropString;
	const TCHAR* Buffer;
	float OrigBeginTime;
	float OrigLength;
	int32 TrackSpan;
	int32 FirstTrack = -1;
	float ScaleMultiplier = 1.0f;

	if(ReadNotifyPasteHeader(PropString, Buffer, OrigBeginTime, OrigLength, TrackSpan))
	{
		DeselectAllNotifies();

		FScopedTransaction Transaction(LOCTEXT("PasteNotifyEvent", "Paste Anim Notifies"));
		Sequence->Modify();

		if(ClickTime == -1.0f)
		{
			// We want to place the notifies exactly where they were
			ClickTime = OrigBeginTime;
		}

		// Expand the number of tracks if we don't have enough.
		check(TrackSpan > 0);
		if(PasteIdx - (TrackSpan - 1) < 0)
		{
			int32 TracksToAdd = PasteIdx + TrackSpan - 1;
			while(TracksToAdd)
			{
				InsertTrack(PasteIdx++);
				--TracksToAdd;
			}
			NumTracks = NotifyAnimTracks.Num();
			RequestTrack = NotifyAnimTracks[NumTracks - 1 - PasteIdx].Get();
		}

		// Scaling for relative paste
		if(MultiplePasteType == ENotifyPasteMultipleMode::Relative)
		{
			ScaleMultiplier = Sequence->SequenceLength / OrigLength;
		}

		// Process each line of the paste buffer and spawn notifies
		FString CurrentLine;
		while(FParse::Line(&Buffer, CurrentLine))
		{
			int32 OriginalTrack;
			float OrigTime;
			float PasteTime = -1.0f;
			if(FParse::Value(*CurrentLine, TEXT("TrackIndex="), OriginalTrack) && FParse::Value(*CurrentLine, TEXT("AbsTime="), OrigTime))
			{
				FString NotifyExportString;
				CurrentLine.Split(TEXT(","), NULL, &NotifyExportString);

				// Store the first track so we know where to place notifies
				if(FirstTrack < 0)
				{
					FirstTrack = OriginalTrack;
				}
				int32 TrackOffset = OriginalTrack - FirstTrack;

				float TimeOffset = OrigTime - OrigBeginTime;
				float TimeToPaste = ClickTime + TimeOffset * ScaleMultiplier;

				// Have to invert the index here as tracks are stored in reverse
				TSharedPtr<SAnimNotifyTrack> TrackToUse = NotifyAnimTracks[NotifyAnimTracks.Num() - 1 - (PasteIdx + TrackOffset)];

				TrackToUse->PasteSingleNotify(NotifyExportString, TimeToPaste);
			}
		}
	}
}

void SAnimNotifyPanel::OnPropertyChanged(UObject* ChangedObject, FPropertyChangedEvent& PropertyEvent)
{
	// Don't process if it's an interactive change; wait till we receive the final event.
	if(PropertyEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		for(FAnimNotifyEvent& Event : Sequence->Notifies)
		{
			if(Event.Notify == ChangedObject || Event.NotifyStateClass == ChangedObject)
			{
				// If we've changed a notify present in the sequence, refresh our tracks.
				RefreshNotifyTracks();
			}
		}
	}
}

void SAnimNotifyPanel::BindCommands()
{
	check(!UICommandList.IsValid());

	UICommandList = MakeShareable(new FUICommandList);
	const FAnimNotifyPanelCommands& Commands = FAnimNotifyPanelCommands::Get();

	UICommandList->MapAction(
		Commands.DeleteNotify,
		FExecuteAction::CreateSP(this, &SAnimNotifyPanel::OnDeletePressed));
}

FReply SAnimNotifyPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAnimNotifyPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SAnimTrackPanel::OnMouseButtonDown(MyGeometry, MouseEvent);

	bool bLeftButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);

	if(bLeftButton)
	{
		TArray<TSharedPtr<SAnimNotifyNode>> SelectedNodes;
		for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
		{
			Track->AppendSelectedNodeWidgetsToArray(SelectedNodes);
		}

		Marquee.Start(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), Marquee.OperationTypeFromMouseEvent(MouseEvent), SelectedNodes);
		if(Marquee.Operation == FNotifyMarqueeOperation::Replace)
		{
			// Remove and Add operations preserve selections, replace starts afresh
			DeselectAllNotifies();
		}

		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return FReply::Unhandled();
}

FReply SAnimNotifyPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(Marquee.bActive)
	{
		OnTrackSelectionChanged();
		Marquee = FNotifyMarqueeOperation();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return SAnimTrackPanel::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SAnimNotifyPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply BaseReply = SAnimTrackPanel::OnMouseMove(MyGeometry, MouseEvent);
	if(!BaseReply.IsEventHandled())
	{
		bool bLeftButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);

		if(bLeftButton && Marquee.bActive)
		{
			Marquee.Rect.UpdateEndPoint(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
			RefreshMarqueeSelectedNodes(MyGeometry);
		}
		return FReply::Handled();
	}

	return BaseReply;
}

int32 SAnimNotifyPanel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SAnimTrackPanel::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FVector2D Origin = AllottedGeometry.AbsoluteToLocal(Marquee.Rect.GetUpperLeft());
	FVector2D Extents = AllottedGeometry.AbsoluteToLocal(Marquee.Rect.GetSize());

	if(Marquee.IsValid())
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(Marquee.Rect.GetUpperLeft(), Marquee.Rect.GetSize()),
			FEditorStyle::GetBrush(TEXT("MarqueeSelection")),
			MyClippingRect
			);
	}

	return LayerId;
}

void SAnimNotifyPanel::RefreshMarqueeSelectedNodes(const FGeometry& PanelGeo)
{
	if(Marquee.IsValid())
	{
		FSlateRect MarqueeRect = Marquee.Rect.ToSlateRect();
		for(TSharedPtr<SAnimNotifyTrack> Track : NotifyAnimTracks)
		{
			if(Marquee.Operation == FNotifyMarqueeOperation::Replace || Marquee.OriginalSelection.Num() == 0)
			{
				Track->DeselectAllNotifyNodes(false);
			}

			const FGeometry& TrackGeo = Track->GetCachedGeometry();

			FSlateRect TrackClip = TrackGeo.GetClippingRect();
			FSlateRect PanelClip = PanelGeo.GetClippingRect();
			FVector2D PanelSpaceOrigin = TrackClip.GetTopLeft() - PanelClip.GetTopLeft();
			FVector2D TrackSpaceOrigin = MarqueeRect.GetTopLeft() - PanelSpaceOrigin;
			FSlateRect MarqueeTrackSpace(TrackSpaceOrigin, TrackSpaceOrigin + MarqueeRect.GetSize());

			Track->RefreshMarqueeSelectedNodes(MarqueeTrackSpace, Marquee);
		}
	}
}

FReply SAnimNotifyPanel::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Marquee.bActive = true;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

void SAnimNotifyPanel::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if(Marquee.bActive)
	{
		OnTrackSelectionChanged();
	}
	Marquee = FNotifyMarqueeOperation();
}

void SAnimNotifyPanel::PopulateNotifyBlueprintClasses(TArray<FString>& InOutAllowedClasses)
{
	TArray<FAssetData> TempArray;
	OnGetNotifyBlueprintData(TempArray, &InOutAllowedClasses);
}

void SAnimNotifyPanel::OnGetNotifyBlueprintData(TArray<FAssetData>& OutNotifyData, TArray<FString>* InOutAllowedClassNames)
{
	// If we have nothing to seach with, early out
	if(InOutAllowedClassNames == NULL || InOutAllowedClassNames->Num() == 0)
	{
		return;
	}

	TArray<FAssetData> AssetDataList;
	TArray<FString> FoundClasses;

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetDataList);

	static const FName BPParentClassName(TEXT("ParentClass"));
	static const FName BPGenClassName(TEXT("GeneratedClass"));

	int32 BeginClassCount = InOutAllowedClassNames->Num();
	int32 CurrentClassCount = -1;

	while(BeginClassCount != CurrentClassCount)
	{
		BeginClassCount = InOutAllowedClassNames->Num();

		for(int32 AssetIndex = 0; AssetIndex < AssetDataList.Num(); ++AssetIndex)
		{
			FAssetData& AssetData = AssetDataList[AssetIndex];
			FString TagValue = AssetData.TagsAndValues.FindRef(BPParentClassName);

			if(InOutAllowedClassNames->Contains(TagValue))
			{
				FString GenClass = AssetData.TagsAndValues.FindRef(BPGenClassName);

				if(!OutNotifyData.Contains(AssetData))
				{
					// Output the assetdata and record it as found in this request
					OutNotifyData.Add(AssetData);
					FoundClasses.Add(GenClass);
				}

				if(!InOutAllowedClassNames->Contains(GenClass))
				{
					// Expand the class list to account for a new possible parent class found
					InOutAllowedClassNames->Add(GenClass);
				}
			}
		}

		CurrentClassCount = InOutAllowedClassNames->Num();
	}

	// Count native classes, so we don't remove them from the list
	int32 NumNativeClasses = 0;
	for(FString& AllowedClass : *InOutAllowedClassNames)
	{
		if(!AllowedClass.EndsWith(FString(TEXT("_C'"))))
		{
			++NumNativeClasses;
		}
	}

	if(FoundClasses.Num() < InOutAllowedClassNames->Num() - NumNativeClasses)
	{
		// Less classes found, some may have been deleted or reparented
		for(int32 ClassIndex = InOutAllowedClassNames->Num() - 1 ; ClassIndex >= 0 ; --ClassIndex)
		{
			FString& ClassName = (*InOutAllowedClassNames)[ClassIndex];
			if(ClassName.EndsWith(FString(TEXT("_C'"))) && !FoundClasses.Contains(ClassName))
			{
				InOutAllowedClassNames->RemoveAt(ClassIndex);
			}
		}
	}
}

void SAnimNotifyPanel::OnGetNativeNotifyData(TArray<UClass*>& OutClasses, UClass* NotifyOutermost, TArray<FString>* OutAllowedBlueprintClassNames)
{
	for(TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* Class = *It;

		if(Class->IsChildOf(NotifyOutermost) && Class->HasAllClassFlags(CLASS_Native) && !Class->HasAllClassFlags(CLASS_Abstract) && !Class->IsInBlueprint())
		{
			OutClasses.Add(Class);
			// Form class name to search later
			FString ClassName = FString::Printf(TEXT("%s'%s'"), *Class->GetClass()->GetName(), *Class->GetPathName());
			OutAllowedBlueprintClassNames->AddUnique(ClassName);
		}
	}
}

void SAnimNotifyPanel::OnNotifyObjectChanged(UObject* EditorBaseObj, bool bRebuild)
{
	if(UEditorNotifyObject* NotifyObject = Cast<UEditorNotifyObject>(EditorBaseObj))
	{
		// TODO: We should really un-invert these.
		int32 WidgetTrackIdx = NotifyAnimTracks.Num() - NotifyObject->TrackIndex - 1;
		if(NotifyAnimTracks.IsValidIndex(WidgetTrackIdx))
		{
			NotifyAnimTracks[WidgetTrackIdx]->Update();
		}
	}
}

void SAnimNotifyPanel::OnNotifyTrackScrolled(float InScrollOffsetFraction)
{
	float Ratio = (ViewInputMax.Get() - ViewInputMin.Get()) / Sequence->SequenceLength;
	float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	// Calculate new view ranges
	float NewMin = InScrollOffsetFraction * Sequence->SequenceLength;
	float NewMax = (InScrollOffsetFraction + Ratio) * Sequence->SequenceLength;
	
	InputViewRangeChanged(NewMin, NewMax);
}

void SAnimNotifyPanel::InputViewRangeChanged(float ViewMin, float ViewMax)
{
	float Ratio = (ViewMax - ViewMin) / Sequence->SequenceLength;
	float OffsetFraction = ViewMin / Sequence->SequenceLength;
	NotifyTrackScrollBar->SetState(OffsetFraction, Ratio);

	SAnimTrackPanel::InputViewRangeChanged(ViewMin, ViewMax);
}

#undef LOCTEXT_NAMESPACE
