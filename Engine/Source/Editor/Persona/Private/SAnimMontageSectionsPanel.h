// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "STrack.h"

class SAnimMontageSectionsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimMontageSectionsPanel )
		: _Montage()
		, _MontageEditor()
	{}

	SLATE_ARGUMENT( class UAnimMontage*, Montage)
		SLATE_ARGUMENT( TWeakPtr<class SMontageEditor>, MontageEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Update();

	void SetSectionPos(float NewPos, int32 SectionIdx, int32 RowIdx);
	void OnSectionDrop();

	void TopSectionClicked(int32 SectionIndex);
	void SectionClicked(int32 SectionIndex);
	void RemoveLink(int32 SectionIndex);

	FReply MakeDefaultSequence();
	FReply ClearSequence();

	FReply PreviewAllSectionsClicked();
	FReply PreviewSectionClicked(int32 SectionIndex);

private:

	TSharedPtr<SBorder> PanelArea;

	class UAnimMontage*			Montage;
	TWeakPtr<SMontageEditor>	MontageEditor;

	TArray<TArray<int32>>		SectionMap;

	STrackNodeSelectionSet TopSelectionSet;
	STrackNodeSelectionSet SelectionSet;

	int32 SelectedCompositeSection;

	bool IsLoop(int32 SectionIdx);
};