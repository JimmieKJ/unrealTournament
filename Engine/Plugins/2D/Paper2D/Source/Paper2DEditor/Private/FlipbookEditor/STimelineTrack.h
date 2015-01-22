// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ScopedTransaction.h"
#include "SFlipbookTimeline.h"

namespace FFlipbookUIConstants
{
	const float HandleWidth = 12.0f;
	const float FrameHeight = 48;
	const FMargin FramePadding(0.0f, 7.0f, 0.0f, 7.0f);
};

//////////////////////////////////////////////////////////////////////////
// FFlipbookKeyFrameDragDropOp

class FFlipbookKeyFrameDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FFlipbookKeyFrameDragDropOp, FDragDropOperation)

	float WidgetWidth;
	FPaperFlipbookKeyFrame KeyFrameData;
	int32 SourceFrameIndex;
	TWeakObjectPtr<UPaperFlipbook> SourceFlipbook;
	FScopedTransaction Transaction;

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	virtual void OnDragged(const class FDragDropEvent& DragDropEvent) override;

	virtual void Construct() override;

	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override;

	void AppendToFlipbook(UPaperFlipbook* DestinationFlipbook);

	void InsertInFlipbook(UPaperFlipbook* DestinationFlipbook, int32 Index);

	void SetCanDropHere(bool bCanDropHere)
	{
		MouseCursor = bCanDropHere ? EMouseCursor::TextEditBeam : EMouseCursor::SlashedCircle;
	}

	static TSharedRef<FFlipbookKeyFrameDragDropOp> New(int32 InWidth, UPaperFlipbook* InFlipbook, int32 InFrameIndex);

protected:
	FFlipbookKeyFrameDragDropOp();
};

//////////////////////////////////////////////////////////////////////////
// SFlipbookKeyframeWidget

class SFlipbookKeyframeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlipbookKeyframeWidget)
		: _SlateUnitsPerFrame(1)
		, _FlipbookBeingEdited(nullptr)
	{}

		SLATE_ATTRIBUTE( float, SlateUnitsPerFrame )
		SLATE_ATTRIBUTE( class UPaperFlipbook*, FlipbookBeingEdited )
		SLATE_EVENT( FOnFlipbookKeyframeSelectionChanged, OnSelectionChanged )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, int32 InFrameIndex, TSharedPtr<const FUICommandList> InCommandList);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	FReply KeyframeOnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	FText GetKeyframeTooltip() const;

	TSharedRef<SWidget> GenerateContextMenu();

	FOptionalSize GetFrameWidth() const;

protected:
	int32 FrameIndex;
	TAttribute<float> SlateUnitsPerFrame;
	TAttribute<class UPaperFlipbook*> FlipbookBeingEdited;
	FOnFlipbookKeyframeSelectionChanged OnSelectionChanged;
	TSharedPtr<const FUICommandList> CommandList;
};

//////////////////////////////////////////////////////////////////////////
// SFlipbookTimelineTrack

class SFlipbookTimelineTrack : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlipbookTimelineTrack)
		: _SlateUnitsPerFrame(1)
		, _FlipbookBeingEdited(nullptr)
	{}

		SLATE_ATTRIBUTE( float, SlateUnitsPerFrame )
		SLATE_ATTRIBUTE( class UPaperFlipbook*, FlipbookBeingEdited )
		SLATE_EVENT( FOnFlipbookKeyframeSelectionChanged, OnSelectionChanged )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<const FUICommandList> InCommandList);

	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		UPaperFlipbook* Flipbook = FlipbookBeingEdited.Get();
		int32 NewNumKeyframes = (Flipbook != nullptr) ? Flipbook->GetNumKeyFrames() : 0;
		if (NewNumKeyframes != NumKeyframesFromLastRebuild)
		{
			Rebuild();
		}
	}

private:
	void Rebuild();

private:
	TAttribute<float> SlateUnitsPerFrame;
	TAttribute< class UPaperFlipbook* > FlipbookBeingEdited;

	TSharedPtr<SHorizontalBox> MainBoxPtr;

	int32 NumKeyframesFromLastRebuild;
	float HandleWidth;

	FOnFlipbookKeyframeSelectionChanged OnSelectionChanged;
	TSharedPtr<const FUICommandList> CommandList;
};
