// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
class SWorldHierarchy
	: public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SWorldHierarchy)
		:_InWorld(nullptr)
		{}
		SLATE_ARGUMENT(UWorld*, InWorld)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	SWorldHierarchy();
	~SWorldHierarchy();

private:
	void OnBrowseWorld(UWorld* InWorld);

	/**  */
	const FSlateBrush* GetLevelsMenuBrush() const;

	/**  */
	FReply OnSummonDetails();
	const FSlateBrush* GetSummonDetailsBrush() const;

	/**  */
	EVisibility GetCompositionButtonVisibility() const;
	FReply OnSummonComposition();
	const FSlateBrush* GetSummonCompositionBrush() const;

	/**  */
	TSharedRef<SWidget> GetFileButtonContent();

private:
	TSharedPtr<FLevelCollectionModel> WorldModel;
};
