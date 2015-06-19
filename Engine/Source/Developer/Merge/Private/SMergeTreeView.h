// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BlueprintMergeData.h"
#include "SCSDiff.h"

class SMergeTreeView	: public SCompoundWidget
{
public:
	virtual ~SMergeTreeView() {}

	SLATE_BEGIN_ARGS(SMergeTreeView)
	{}
	SLATE_END_ARGS()

	void Construct(	const FArguments InArgs
					, const FBlueprintMergeData& InData
					, FOnMergeNodeSelected SelectionCallback
					, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries
					, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences
					, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutConflicts);
private:
	void HighlightDifference( FSCSIdentifier VarName, FPropertySoftPath Property );

	TSharedRef<FSCSDiff>& GetRemoteView();
	TSharedRef<FSCSDiff>& GetBaseView();
	TSharedRef<FSCSDiff>& GetLocalView();

	FBlueprintMergeData Data;
	TArray< TSharedRef<FSCSDiff> > SCSViews;

	FSCSDiffRoot MergeConflicts;
	int CurrentMergeConflict;

	FSCSDiffRoot DifferingProperties;
	int CurrentDifference;
};
