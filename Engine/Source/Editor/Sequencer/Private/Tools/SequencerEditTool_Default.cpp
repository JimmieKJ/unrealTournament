// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerEditTool_Default.h"
#include "Sequencer.h"
#include "SequencerHotspots.h"
#include "SequencerContextMenus.h"
#include "CommonMovieSceneTools.h"
#include "VirtualTrackArea.h"


FReply FSequencerEditTool_Default::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FSequencerSelection& Selection = GetSequencer().GetSelection();
		Selection.EmptySelectedSections();
		Selection.EmptySelectedKeys();
	}

	return FReply::Unhandled();
}

void FSequencerEditTool_Default::PerformHotspotSelection(const FPointerEvent& MouseEvent)
{
	FSequencerSelection& Selection = GetSequencer().GetSelection();

	// @todo: selection in transactions
	auto ConditionallyClearSelection = [&]{
		if (!MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown())
		{
			Selection.EmptySelectedSections();
			Selection.EmptySelectedKeys();
		}
	};

	// Handle right-click selection separately since we never deselect on right click (except for clearing on exclusive selection)
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (Hotspot->GetType() == ESequencerHotspot::Key)
		{
			FSequencerSelectedKey Key = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
			if (!Selection.IsSelected(Key))
			{
				ConditionallyClearSelection();
				Selection.AddToSelection(Key);
			}
		}
		else if (Hotspot->GetType() == ESequencerHotspot::Section)
		{
			UMovieSceneSection* Section = static_cast<FSectionHotspot*>(Hotspot.Get())->Section.GetSectionObject();
			if (!Selection.IsSelected(Section))
			{
				ConditionallyClearSelection();
				Selection.AddToSelection(Section);
			}
		}

		return;
	}

	// Normal selection
	ConditionallyClearSelection();

	bool bForceSelect = !MouseEvent.IsControlDown();

	if (Hotspot->GetType() == ESequencerHotspot::Key)
	{
		FSequencerSelectedKey Key = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
		if (bForceSelect || !Selection.IsSelected(Key))
		{
			Selection.AddToSelection(Key);
		}
		else
		{
			Selection.RemoveFromSelection(Key);
		}
	}
	else if (Hotspot->GetType() == ESequencerHotspot::Section)
	{
		UMovieSceneSection* Section = static_cast<FSectionHotspot*>(Hotspot.Get())->Section.GetSectionObject();
		if (bForceSelect || !Selection.IsSelected(Section))
		{
			Selection.AddToSelection(Section);
		}
		else
		{
			Selection.RemoveFromSelection(Section);
		}
	}
}

TSharedPtr<SWidget> FSequencerEditTool_Default::OnSummonContextMenu( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// @todo sequencer replace with UI Commands instead of faking it

	FSequencer& Sequencer = static_cast<FSequencer&>(GetSequencer());
	FVector2D MouseDownPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Attempt to paste into either the current node selection, or the clicked on track
	TSharedRef<SSequencer> SequencerWidget = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget());
	const float PasteAtTime = SequencerWidget->GetVirtualTrackArea().PixelToTime(MouseDownPos.X);

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, Sequencer.GetCommandBindings());

	if (Hotspot.IsValid())
	{
		if (Hotspot->GetType() == ESequencerHotspot::Section || Hotspot->GetType() == ESequencerHotspot::Key)
		{
			Hotspot->PopulateContextMenu(MenuBuilder, Sequencer, PasteAtTime);

			return MenuBuilder.MakeWidget();
		}
	}
	else if (Sequencer.GetClipboardStack().Num() != 0)
	{
		TSharedPtr<FPasteContextMenu> PasteMenu = FPasteContextMenu::CreateMenu(Sequencer, SequencerWidget->GeneratePasteArgs(PasteAtTime));
		if (PasteMenu.IsValid() && PasteMenu->IsValidPaste())
		{
			PasteMenu->PopulateMenu(MenuBuilder);

			return MenuBuilder.MakeWidget();
		}
	}

	return nullptr;
}