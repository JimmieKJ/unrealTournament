// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FSequencer;


/**
* Wrapper box to encompass clicks that don't fall on a tree view row.
*/
class SSequencerTreeViewBox
	: public SBox
{
public:

	void Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer)
	{
		Sequencer = InSequencer;
		SBox::Construct(InArgs);
	}
	
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override 
	{
		if (Sequencer.Pin().IsValid())
		{
			FSequencerSelection& Selection = Sequencer.Pin()->GetSelection();

			if (Selection.GetSelectedOutlinerNodes().Num())
			{
				Selection.EmptySelectedOutlinerNodes();
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}
	
private:

	/** Weak pointer to the sequencer */
	TWeakPtr<FSequencer> Sequencer;
};
