// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

class FLevelCollectionModel;
//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
class SWorldDetails
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldDetails)
		:_InWorld(nullptr)
		{}
		SLATE_ARGUMENT(UWorld*, InWorld)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	SWorldDetails();
	~SWorldDetails();

private:
	/**  */
	void OnBrowseWorld(UWorld* InWorld);

	/** Handles selection changes in data source */
	void OnSelectionChanged();

	/** Handles levels collection changes in data source */
	void OnCollectionChanged();

	/**  */
	void OnSetInspectedLevel(TSharedPtr<FLevelModel> InLevelModel, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> HandleInspectedLevelComboBoxGenerateWidget(TSharedPtr<FLevelModel> InLevelModel) const;
	FText GetInspectedLevelText() const;
	
	/**  */
	FReply OnSummonHierarchy();
	const FSlateBrush* GetSummonHierarchyBrush() const;

	/**  */
	EVisibility GetCompositionButtonVisibility() const;
	FReply OnSummonComposition();
	const FSlateBrush* GetSummonCompositionBrush() const;
		
private:
	TSharedPtr<FLevelCollectionModel>				WorldModel;
	TSharedPtr<IDetailsView>						DetailsView;
	TSharedPtr<SComboBox<TSharedPtr<FLevelModel>>>	SubLevelsComboBox;
	bool											bUpdatingSelection;	
};
