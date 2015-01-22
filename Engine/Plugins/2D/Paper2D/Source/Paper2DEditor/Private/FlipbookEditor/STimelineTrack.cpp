// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "STimelineTrack.h"
#include "GenericCommands.h"
#include "FlipbookEditorCommands.h"
#include "SFlipbookTrackHandle.h"
#include "AssetDragDropOp.h"

#define LOCTEXT_NAMESPACE "FlipbookEditor"

TSharedRef<SWidget> SFlipbookKeyframeWidget::GenerateContextMenu()
{
	OnSelectionChanged.ExecuteIfBound(FrameIndex);

	FMenuBuilder MenuBuilder(true, CommandList);
	MenuBuilder.BeginSection("KeyframeActions", LOCTEXT("KeyframeActionsSectionHeader", "Keyframe Actions"));

	// 		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);
	// 		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
	// 		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(FFlipbookEditorCommands::Get().AddNewFrameBefore);
	MenuBuilder.AddMenuEntry(FFlipbookEditorCommands::Get().AddNewFrameAfter);

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SFlipbookKeyframeWidget::Construct(const FArguments& InArgs, int32 InFrameIndex, TSharedPtr<const FUICommandList> InCommandList)
{
	FrameIndex = InFrameIndex;
	CommandList = InCommandList;
	SlateUnitsPerFrame = InArgs._SlateUnitsPerFrame;
	FlipbookBeingEdited = InArgs._FlipbookBeingEdited;
	OnSelectionChanged = InArgs._OnSelectionChanged;

	// Color each region based on whether a sprite has been set or not for it
	const auto BorderColorDelegate = [](TAttribute<UPaperFlipbook*> ThisFlipbookPtr, int32 TestIndex) -> FSlateColor
	{
		UPaperFlipbook* FlipbookPtr = ThisFlipbookPtr.Get();
		const bool bFrameValid = (FlipbookPtr != nullptr) && (FlipbookPtr->GetSpriteAtFrame(TestIndex) != nullptr);
		return bFrameValid ? FLinearColor::White : FLinearColor::Black;
	};

	ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.Padding(FFlipbookUIConstants::FramePadding)
				.WidthOverride(this, &SFlipbookKeyframeWidget::GetFrameWidth)
				[
					SNew(SBorder)
					.BorderImage(FPaperStyle::Get()->GetBrush("FlipbookEditor.RegionBody"))
					.BorderBackgroundColor_Static(BorderColorDelegate, FlipbookBeingEdited, FrameIndex)
					.OnMouseButtonUp(this, &SFlipbookKeyframeWidget::KeyframeOnMouseButtonUp)
					.ToolTipText(this, &SFlipbookKeyframeWidget::GetKeyframeTooltip)
					[
						SNullWidget::NullWidget
					]
				]
			]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(FFlipbookUIConstants::HandleWidth)
					[
						SNew(SFlipbookTrackHandle)
						.SlateUnitsPerFrame(SlateUnitsPerFrame)
						.FlipbookBeingEdited(FlipbookBeingEdited)
						.KeyFrameIdx(FrameIndex)
					]
				]
		];
}

FReply SFlipbookKeyframeWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return FReply::Unhandled();
}

FReply SFlipbookKeyframeWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		UPaperFlipbook* Flipbook = FlipbookBeingEdited.Get();
		if ((Flipbook != nullptr) && Flipbook->IsValidKeyFrameIndex(FrameIndex))
		{
			TSharedRef<FFlipbookKeyFrameDragDropOp> Operation = FFlipbookKeyFrameDragDropOp::New(
				GetFrameWidth().Get(), Flipbook, FrameIndex);

			return FReply::Handled().BeginDragDrop(Operation);
		}
	}

	return FReply::Unhandled();
}

FReply SFlipbookKeyframeWidget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bool bWasDropHandled = false;

	UPaperFlipbook* Flipbook = FlipbookBeingEdited.Get();
	if ((Flipbook != nullptr) && Flipbook->IsValidKeyFrameIndex(FrameIndex))
	{

		TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
		if (!Operation.IsValid())
		{
		}
		else if (Operation->IsOfType<FAssetDragDropOp>())
		{
			const auto& AssetDragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
			//@TODO: Handle asset inserts

			// 			OnAssetsDropped(*AssetDragDropOp);
			// 			bWasDropHandled = true;
		}
		else if (Operation->IsOfType<FFlipbookKeyFrameDragDropOp>())
		{
			const auto& FrameDragDropOp = StaticCastSharedPtr<FFlipbookKeyFrameDragDropOp>(Operation);
			FrameDragDropOp->InsertInFlipbook(Flipbook, FrameIndex);
			bWasDropHandled = true;
		}
	}

	return bWasDropHandled ? FReply::Handled() : FReply::Unhandled();
}

FReply SFlipbookKeyframeWidget::KeyframeOnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		TSharedRef<SWidget> MenuContents = GenerateContextMenu();
		FSlateApplication::Get().PushMenu(AsShared(), MenuContents, MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FText SFlipbookKeyframeWidget::GetKeyframeTooltip() const
{
	UPaperFlipbook* Flipbook = FlipbookBeingEdited.Get();
	if ((Flipbook != nullptr) && Flipbook->IsValidKeyFrameIndex(FrameIndex))
	{
		const FPaperFlipbookKeyFrame& KeyFrame = Flipbook->GetKeyFrameChecked(FrameIndex);

		FText SpriteLine = (KeyFrame.Sprite != nullptr) ? FText::FromString(KeyFrame.Sprite->GetName()) : LOCTEXT("NoSprite", "(none)");

		return FText::Format(LOCTEXT("KeyFrameTooltip", "Sprite: {0}\nIndex: {1}\nDuration: {2} frame(s)"),
			SpriteLine,
			FText::AsNumber(FrameIndex),
			FText::AsNumber(KeyFrame.FrameRun));
	}
	else
	{
		return LOCTEXT("KeyFrameTooltip_Invalid", "Invalid key frame index");
	}
}

FOptionalSize SFlipbookKeyframeWidget::GetFrameWidth() const
{
	UPaperFlipbook* Flipbook = FlipbookBeingEdited.Get();
	if (Flipbook && Flipbook->IsValidKeyFrameIndex(FrameIndex))
	{
		const FPaperFlipbookKeyFrame& KeyFrame = Flipbook->GetKeyFrameChecked(FrameIndex);
		return FMath::Max<float>(0, KeyFrame.FrameRun * SlateUnitsPerFrame.Get() - FFlipbookUIConstants::HandleWidth);
	}
	else
	{
		return 1;
	}
}

TSharedPtr<SWidget> FFlipbookKeyFrameDragDropOp::GetDefaultDecorator() const
{
	const FLinearColor BorderColor = (KeyFrameData.Sprite != nullptr) ? FLinearColor::White : FLinearColor::Black;

	return SNew(SBox)
		.WidthOverride(WidgetWidth - FFlipbookUIConstants::FramePadding.GetTotalSpaceAlong<Orient_Horizontal>())
		.HeightOverride(FFlipbookUIConstants::FrameHeight - FFlipbookUIConstants::FramePadding.GetTotalSpaceAlong<Orient_Vertical>())
		[
			SNew(SBorder)
			.BorderImage(FPaperStyle::Get()->GetBrush("FlipbookEditor.RegionBody"))
			.BorderBackgroundColor(BorderColor)
			[
				SNullWidget::NullWidget
			]
		];
}

void FFlipbookKeyFrameDragDropOp::OnDragged(const class FDragDropEvent& DragDropEvent)
{
	if (CursorDecoratorWindow.IsValid())
	{
		CursorDecoratorWindow->MoveWindowTo(DragDropEvent.GetScreenSpacePosition());
	}
}

void FFlipbookKeyFrameDragDropOp::Construct()
{
	MouseCursor = EMouseCursor::GrabHandClosed;
	FDragDropOperation::Construct();
}

void FFlipbookKeyFrameDragDropOp::OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent)
{
	if (!bDropWasHandled)
	{
		// Add us back to our source, the drop fizzled
		InsertInFlipbook(SourceFlipbook.Get(), SourceFrameIndex);
		Transaction.Cancel();
	}
}

void FFlipbookKeyFrameDragDropOp::AppendToFlipbook(UPaperFlipbook* DestinationFlipbook)
{
	DestinationFlipbook->Modify();
	FScopedFlipbookMutator EditLock(DestinationFlipbook);
	EditLock.KeyFrames.Add(KeyFrameData);
}

void FFlipbookKeyFrameDragDropOp::InsertInFlipbook(UPaperFlipbook* DestinationFlipbook, int32 Index)
{
	DestinationFlipbook->Modify();
	FScopedFlipbookMutator EditLock(DestinationFlipbook);
	EditLock.KeyFrames.Insert(KeyFrameData, Index);
}

TSharedRef<FFlipbookKeyFrameDragDropOp> FFlipbookKeyFrameDragDropOp::New(int32 InWidth, UPaperFlipbook* InFlipbook, int32 InFrameIndex)
{
	// Create the drag-drop op containing the key
	TSharedRef<FFlipbookKeyFrameDragDropOp> Operation = MakeShareable(new FFlipbookKeyFrameDragDropOp);
	Operation->KeyFrameData = InFlipbook->GetKeyFrameChecked(InFrameIndex);
	Operation->SourceFrameIndex = InFrameIndex;
	Operation->SourceFlipbook = InFlipbook;
	Operation->WidgetWidth = InWidth;
	Operation->Construct();

	// Remove the key from the flipbook
	{
		InFlipbook->Modify();
		FScopedFlipbookMutator EditLock(InFlipbook);
		EditLock.KeyFrames.RemoveAt(InFrameIndex);
	}

	return Operation;
}

FFlipbookKeyFrameDragDropOp::FFlipbookKeyFrameDragDropOp() : Transaction(LOCTEXT("MovedFramesInTimeline", "Reorder key frames"))
{

}

void SFlipbookTimelineTrack::Construct(const FArguments& InArgs, TSharedPtr<const FUICommandList> InCommandList)
{
	CommandList = InCommandList;
	SlateUnitsPerFrame = InArgs._SlateUnitsPerFrame;
	FlipbookBeingEdited = InArgs._FlipbookBeingEdited;
	OnSelectionChanged = InArgs._OnSelectionChanged;

	NumKeyframesFromLastRebuild = 0;

	ChildSlot
		[
			SAssignNew(MainBoxPtr, SHorizontalBox)
		];

	Rebuild();
}

void SFlipbookTimelineTrack::Rebuild()
{
	MainBoxPtr->ClearChildren();

	// Create the sections for each keyframe
	if (UPaperFlipbook* Flipbook = FlipbookBeingEdited.Get())
	{
		for (int32 KeyFrameIdx = 0; KeyFrameIdx < Flipbook->GetNumKeyFrames(); ++KeyFrameIdx)
		{
			//@TODO: Draggy bits go here
			MainBoxPtr->AddSlot()
				.AutoWidth()
				[
					SNew(SFlipbookKeyframeWidget, KeyFrameIdx, CommandList)
					.SlateUnitsPerFrame(this->SlateUnitsPerFrame)
					.FlipbookBeingEdited(this->FlipbookBeingEdited)
					.OnSelectionChanged(this->OnSelectionChanged)
				];
		}

		NumKeyframesFromLastRebuild = Flipbook->GetNumKeyFrames();
	}
	else
	{
		NumKeyframesFromLastRebuild = 0;
	}
}

#undef LOCTEXT_NAMESPACE
