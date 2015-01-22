// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class SPropertySceneOutliner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPropertySceneOutliner ) {}
		SLATE_EVENT( FOnGetActorFilters, OnGetActorFilters )
		SLATE_EVENT( FOnActorSelected, OnActorSelected )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
private:

	FReply OnClicked();

	TSharedRef<SWidget> OnGenerateSceneOutliner();

	void OnActorSelectedFromOutliner( AActor* InActor );

private:
	/** Menu anchor for opening and closing the scene outliner */
	TSharedPtr< class SMenuAnchor > SceneOutlinerAnchor;

	FOnGetActorFilters OnGetActorFilters;
	FOnActorSelected OnActorSelected;
};